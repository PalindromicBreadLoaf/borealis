/*
    Copyright 2021 natinusala

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <math.h>

#include <borealis/core/application.hpp>
#include <borealis/platforms/switch/switch_input.hpp>

namespace brls
{

static const uint64_t HidNpadButton_None = ((uint64_t)1 << (63));

static const uint64_t SWITCH_BUTTONS_FULL_MAPPING[_BUTTON_MAX] = {
    HidNpadButton_ZL, // BUTTON_LT
    HidNpadButton_L, // BUTTON_LB

    HidNpadButton_StickL, // BUTTON_LSB

    HidNpadButton_Up, // BUTTON_UP
    HidNpadButton_Right, // BUTTON_RIGHT
    HidNpadButton_Down, // BUTTON_DOWN
    HidNpadButton_Left, // BUTTON_LEFT

    HidNpadButton_Minus, // BUTTON_BACK
    HidNpadButton_None, // BUTTON_GUIDE
    HidNpadButton_Plus, // BUTTON_START

    HidNpadButton_StickR, // BUTTON_RSB

    HidNpadButton_Y, // BUTTON_Y
    HidNpadButton_B, // BUTTON_B
    HidNpadButton_A, // BUTTON_A
    HidNpadButton_X, // BUTTON_X

    HidNpadButton_R, // BUTTON_RB
    HidNpadButton_ZR, // BUTTON_RT

    HidNpadButton_AnyUp, // BUTTON_NAV_UP
    HidNpadButton_AnyRight, // BUTTON_NAV_RIGHT
    HidNpadButton_AnyDown, // BUTTON_NAV_DOWN
    HidNpadButton_AnyLeft, // BUTTON_NAV_LEFT
};

static const uint64_t SWITCH_BUTTONS_HALF_MAPPING[_BUTTON_MAX] = {
    HidNpadButton_None, // BUTTON_LT
    HidNpadButton_AnySL, // BUTTON_LB

    HidNpadButton_StickL | HidNpadButton_StickR, // BUTTON_LSB

    // HidNpadButton_StickLRight | HidNpadButton_StickRLeft, // BUTTON_UP
    // HidNpadButton_StickLDown  | HidNpadButton_StickRUp, // BUTTON_RIGHT
    // HidNpadButton_StickLLeft  | HidNpadButton_StickRRight, // BUTTON_DOWN
    // HidNpadButton_StickLUp    | HidNpadButton_StickRDown, // BUTTON_LEFT

    HidNpadButton_None, // BUTTON_UP
    HidNpadButton_None, // BUTTON_RIGHT
    HidNpadButton_None, // BUTTON_DOWN
    HidNpadButton_None, // BUTTON_LEFT

    HidNpadButton_None, // BUTTON_BACK
    HidNpadButton_None, // BUTTON_GUIDE
    HidNpadButton_Plus | HidNpadButton_Minus, // BUTTON_START

    HidNpadButton_None, // BUTTON_RSB

    HidNpadButton_B | HidNpadButton_Up, // BUTTON_Y
    HidNpadButton_A | HidNpadButton_Left, // BUTTON_B
    HidNpadButton_X | HidNpadButton_Down, // BUTTON_A
    HidNpadButton_Y | HidNpadButton_Right, // BUTTON_X

    HidNpadButton_AnySR, // BUTTON_RB
    HidNpadButton_None, // BUTTON_RT

    HidNpadButton_StickLRight | HidNpadButton_StickRLeft, // BUTTON_NAV_UP
    HidNpadButton_StickLDown | HidNpadButton_StickRUp, // BUTTON_NAV_RIGHT
    HidNpadButton_StickLLeft | HidNpadButton_StickRRight, // BUTTON_NAV_DOWN
    HidNpadButton_StickLUp | HidNpadButton_StickRDown, // BUTTON_NAV_LEFT
};

static const size_t SWITCH_AXIS_MAPPING[_AXES_MAX] = {
    LEFT_X,
    LEFT_Y,
    RIGHT_X,
    RIGHT_Y,
};

SwitchInputManager::SwitchInputManager()
{
    padConfigureInput(GAMEPADS_MAX, HidNpadStyleSet_NpadStandard);
    hidSetNpadJoyHoldType(HidNpadJoyHoldType_Horizontal);
    hidSetNpadHandheldActivationMode(HidNpadHandheldActivationMode_Single);

    padInitialize(&this->padStateHandheld, HidNpadIdType_Handheld);
    hidInitializeVibrationDevices(m_vibration_device_handheld, 2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    padUpdate(&this->padStateHandheld);

    for (int i = 0; i < GAMEPADS_MAX; i++)
    {
        padInitialize(&this->padsState[i], (HidNpadIdType)i);
        padUpdate(&this->padsState[i]);
        padsStyleSet[i] = this->padsState[i].style_set;
        clearVibration(i);
    }

    hidInitializeMouse();
    hidInitializeKeyboard();
    hidInitializeKeyboardMap();

    m_hid_keyboard_state.assign(256, false);

    // It's necessary to initialize these separately as they all have different handle values
    hidGetSixAxisSensorHandles(&this->m_six_axis_sensor_handle[0], 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    hidGetSixAxisSensorHandles(&this->m_six_axis_sensor_handle[1], 1, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey);
    hidGetSixAxisSensorHandles(&this->m_six_axis_sensor_handle[2], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
    hidStartSixAxisSensor(this->m_six_axis_sensor_handle[0]);
    hidStartSixAxisSensor(this->m_six_axis_sensor_handle[1]);
    hidStartSixAxisSensor(this->m_six_axis_sensor_handle[2]);
    hidStartSixAxisSensor(this->m_six_axis_sensor_handle[3]);

#ifdef __SWITCH2__
    u64 aruid = appletGetAppletResourceUserId();
    Result rc = serviceDispatchIn(hidsysGetServiceSession(), 500, aruid);
    if (R_FAILED(rc))
        Logger::warning("SixAxis: hidsys applet resource binding failed: 0x{:x}", (unsigned)rc);
#endif
}

SwitchInputManager::~SwitchInputManager()
{
    NVGcontext* vg = Application::getNVGContext();

    if (this->cursorTexture != 0)
        nvgDeleteImage(vg, this->cursorTexture);

    hidStopSixAxisSensor(this->m_six_axis_sensor_handle[0]);
    hidStopSixAxisSensor(this->m_six_axis_sensor_handle[1]);
    hidStopSixAxisSensor(this->m_six_axis_sensor_handle[2]);
    hidStopSixAxisSensor(this->m_six_axis_sensor_handle[3]);

#ifdef __SWITCH2__
    if (this->padPointerReady)
        padPointerExit(&this->padPointer);
#endif
}

void SwitchInputManager::setPadMouseEnabled(bool enabled)
{
#ifdef __SWITCH2__
    if (this->padMouseEnabled == enabled)
        return;
    this->padMouseEnabled = enabled;
    this->exitPadMouseMode();
    this->padMouseHasAnchor = false;
    if (enabled)
    {
        if (R_SUCCEEDED(padPointerInitialize(&this->padPointer, PAD_MOUSE_AREA_W, PAD_MOUSE_AREA_H)))
        {
            // Map R/ZR (right Joy-Con) and L/ZL (left Joy-Con) to the
            // left/right click groups; the stock config maps both to
            // left, which leaves right click unreachable.
            HidNmouseButtonConfigJoyRightJcms rightConfig { };
            rightConfig.selectors[0] = 1; // R  -> group 1 (Left)
            rightConfig.selectors[1] = 2; // ZR -> group 2 (Right)
            HidNmouseButtonConfigJoyLeftJcms leftConfig { };
            leftConfig.selectors[0] = 1; // L  -> group 1 (Left)
            leftConfig.selectors[1] = 2; // ZL -> group 2 (Right)
            if (R_SUCCEEDED(hidSetNmouseButtonConfigJoyRightJcms(&rightConfig))
                && R_SUCCEEDED(hidSetNmouseButtonConfigJoyLeftJcms(&leftConfig)))
            {
                this->padPointerReady = true;
                Logger::info("PadMouse: Joy-Con pointer enabled");
            }
            else
            {
                padPointerExit(&this->padPointer);
                Logger::warning("PadMouse: button config failed, disabled");
            }
        }
        else
        {
            Logger::warning("PadMouse: padPointerInitialize failed, disabled");
        }
    }
    else if (this->padPointerReady)
    {
        padPointerExit(&this->padPointer);
        this->padPointerReady = false;
        Logger::info("PadMouse: disabled, sticks back on the pads");
    }
#else
    (void)enabled;
#endif
}

void SwitchInputManager::setPadMouseBindings(std::vector<ControllerButton> buttons)
{
#ifdef __SWITCH2__
    for (size_t i = 0; i < _BUTTON_MAX; i++)
        this->padMouseBindings[i] = false;
    for (ControllerButton button : buttons)
    {
        if (button >= 0 && button < _BUTTON_MAX)
            this->padMouseBindings[button] = true;
    }
#else
    (void)buttons;
#endif
}

bool SwitchInputManager::isPadMouseBinding(ControllerButton button)
{
#ifdef __SWITCH2__
    return this->padMouseMode && button >= 0 && button < _BUTTON_MAX && this->padMouseBindings[button];
#else
    (void)button;
    return false;
#endif
}

bool SwitchInputManager::isPadMouseMode()
{
#ifdef __SWITCH2__
    return this->padMouseMode;
#else
    return false;
#endif
}

std::string SwitchInputManager::getSensorDebugInfo()
{
#ifdef __SWITCH2__
    const HidSixAxisSensorState left  = this->readSixAxis(this->m_six_axis_sensor_handle[2]);
    const HidSixAxisSensorState right = this->readSixAxis(this->m_six_axis_sensor_handle[3]);

    auto grip = [](float x, bool isLeft)
    {
        if (isLeft)
            return x >= PAD_MOUSE_GRIP_MIN_ACCEL && x <= PAD_MOUSE_GRIP_MAX_ACCEL;
        return x >= -PAD_MOUSE_GRIP_MAX_ACCEL && x <= -PAD_MOUSE_GRIP_MIN_ACCEL;
    };

    return fmt::format(
        "LC accel({:+.2f},{:+.2f},{:+.2f}) {}\n"
        "RC accel({:+.2f},{:+.2f},{:+.2f}) {}",
        left.acceleration.x, left.acceleration.y, left.acceleration.z,
        grip(left.acceleration.x, true) ? "grip" : "----",
        right.acceleration.x, right.acceleration.y, right.acceleration.z,
        grip(right.acceleration.x, false) ? "grip" : "----");
#else
    return "";
#endif
}

#ifdef __SWITCH2__
void SwitchInputManager::exitPadMouseMode()
{
    if (!this->padMouseMode)
        return;
    this->padMouseMode    = false;
    this->padMouseAnchorX = this->padPointer.x;
    this->padMouseAnchorY = this->padPointer.y;
    // Hand control back to the pads: the input type stays TOUCH from the
    // last mouse activity otherwise, and gamepad focus behaviour (list
    // re-centering etc.) is gated on the GAMEPAD type in borealis.
    Application::setInputType(InputType::GAMEPAD);
}

bool SwitchInputManager::padMouseGripOk()
{
    const HidSixAxisSensorState left  = this->readSixAxis(this->m_six_axis_sensor_handle[2]);
    const HidSixAxisSensorState right = this->readSixAxis(this->m_six_axis_sensor_handle[3]);
    const float lx                    = left.acceleration.x;
    const float rx                    = right.acceleration.x;
    const bool leftGrip               = lx >= PAD_MOUSE_GRIP_MIN_ACCEL && lx <= PAD_MOUSE_GRIP_MAX_ACCEL;
    const bool rightGrip              = rx >= -PAD_MOUSE_GRIP_MAX_ACCEL && rx <= -PAD_MOUSE_GRIP_MIN_ACCEL;
    return leftGrip || rightGrip;
}

HidSixAxisSensorState SwitchInputManager::readSixAxis(HidSixAxisSensorHandle handle)
{
    HidSixAxisSensorState s = { 0 };
    hidGetSixAxisSensorStates(handle, &s, 1);
    if (s.acceleration.x == 0.f && s.acceleration.y == 0.f && s.acceleration.z == -1.f)
    {
        u64 aruid = appletGetAppletResourceUserId();
        serviceDispatchIn(hidsysGetServiceSession(), 500, aruid);
        hidGetSixAxisSensorStates(handle, &s, 1);
    }
    return s;
}
#endif

void SwitchInputManager::clearVibration(int controller)
{
    Logger::debug("Vibration clear #{}", controller);
    hidInitializeVibrationDevices(m_vibration_device_handles[controller], 2, (HidNpadIdType)controller, HidNpadStyleTag_NpadJoyDual);
    sendRumbleInternal(m_vibration_device_handles[controller], m_vibration_values[controller], 160.0f, 320.0f, 0.0f, 0.0f);
}

void SwitchInputManager::updateUnifiedControllerState(ControllerState* state)
{
    for (size_t i = 0; i < _BUTTON_MAX; i++)
        state->buttons[i] = false;

    for (size_t i = 0; i < _AXES_MAX; i++)
        state->axes[i] = 0;

    auto connected = getControllersConnectedCount();
    for (int i = 0; i < connected; i++)
    {
        ControllerState localState;
        updateControllerState(&localState, i);
        for (size_t j = 0; j < _BUTTON_MAX; j++)
            state->buttons[j] |= localState.buttons[j];

        for (size_t j = 0; j < _AXES_MAX; j++)
        {
            state->axes[j] += localState.axes[j];

            if (state->axes[j] < -1)
                state->axes[j] = -1;
            else if (state->axes[j] > 1)
                state->axes[j] = 1;
        }
    }

    state->buttons[BUTTON_NAV_UP] |= getKeyboardKeyState(BRLS_KBD_KEY_UP);
    state->buttons[BUTTON_NAV_RIGHT] |= getKeyboardKeyState(BRLS_KBD_KEY_RIGHT);
    state->buttons[BUTTON_NAV_DOWN] |= getKeyboardKeyState(BRLS_KBD_KEY_DOWN);
    state->buttons[BUTTON_NAV_LEFT] |= getKeyboardKeyState(BRLS_KBD_KEY_LEFT);

    if (Application::isSwapInputKeys())
    {
        state->buttons[BUTTON_B] |= getKeyboardKeyState(BRLS_KBD_KEY_ENTER);
        state->buttons[BUTTON_A] |= getKeyboardKeyState(BRLS_KBD_KEY_ESCAPE);
    }
    else
    {
        state->buttons[BUTTON_A] |= getKeyboardKeyState(BRLS_KBD_KEY_ENTER);
        state->buttons[BUTTON_B] |= getKeyboardKeyState(BRLS_KBD_KEY_ESCAPE);
    }

#ifdef __SWITCH2__
    if (this->padMouseMode)
    {
        for (size_t j = 0; j < _BUTTON_MAX; j++)
        {
            if (state->buttons[j] && !this->padMouseBindings[j])
            {
                this->exitPadMouseMode();
                break;
            }
        }
    }
#endif
}

short SwitchInputManager::getControllersConnectedCount()
{
    padUpdate(&this->padStateHandheld);
    int extra       = padStateHandheld.active_handheld ? 1 : 0;
    int controllers = extra;
    for (int i = 0; i < GAMEPADS_MAX - extra; i++)
    {
        padUpdate(&this->padsState[i]);
        if (padsState[i].style_set == 0)
            break;
        controllers++;
    }
    return controllers;
}

void SwitchInputManager::updateControllerState(ControllerState* state, int controller)
{
    padUpdate(&this->padStateHandheld);
    if (controller == 0 && padStateHandheld.active_handheld)
    {
        updateControllerStateInner(state, &padStateHandheld);
        return;
    }

    int localController = padStateHandheld.active_handheld ? controller - 1 : controller;
    PadState* pad       = &this->padsState[localController];
    updateControllerStateInner(state, pad);

    if (padsStyleSet[localController] != pad->style_set)
    {
        padsStyleSet[localController] = pad->style_set;
        clearVibration(localController);
    }
}

void SwitchInputManager::updateControllerStateInner(ControllerState* state, PadState* pad)
{
    padUpdate(pad);
    uint64_t keysDown = padGetButtons(pad);

#ifdef __SWITCH2__
    if (this->padMouseMode)
        keysDown &= ~PAD_MOUSE_CLICK_MASK;
#endif

    bool full = pad->style_set & HidNpadStyleSet_NpadFullCtrl;

    for (size_t i = 0; i < _BUTTON_MAX; i++)
    {
        uint64_t switchKey = full ? SWITCH_BUTTONS_FULL_MAPPING[i] : SWITCH_BUTTONS_HALF_MAPPING[i];
        state->buttons[i]  = keysDown & switchKey;
    }

    HidAnalogStickState analog_stick_l = padGetStickPos(pad, 0);
    HidAnalogStickState analog_stick_r = padGetStickPos(pad, 1);

    if (full)
    {
        state->axes[LEFT_X]  = (float)analog_stick_l.x / (float)0x7FFF;
        state->axes[LEFT_Y]  = (float)analog_stick_l.y / (float)0x7FFF * -1.0f;
        state->axes[RIGHT_X] = (float)analog_stick_r.x / (float)0x7FFF;
        state->axes[RIGHT_Y] = (float)analog_stick_r.y / (float)0x7FFF * -1.0f;
    }
    else
    {
        state->axes[LEFT_X]  = (float)analog_stick_l.y / (float)0x7FFF * -1.0f + (float)analog_stick_r.y / (float)0x7FFF;
        state->axes[LEFT_Y]  = (float)analog_stick_l.x / (float)0x7FFF * -1.0f + (float)analog_stick_r.x / (float)0x7FFF;
        state->axes[RIGHT_X] = 0;
        state->axes[RIGHT_Y] = 0;
    }

    state->axes[LEFT_Z]  = 0; // SWITCH NOT SUPPORT ZL AXIS
    state->axes[RIGHT_Z] = 0; // SWITCH NOT SUPPORT ZR AXIS
}

bool SwitchInputManager::getKeyboardKeyState(BrlsKeyboardScancode key)
{
    return m_hid_keyboard_state[m_brls_hid_map[key]];
}

void SwitchInputManager::updateTouchStates(std::vector<RawTouchState>* states)
{
    // Get touchscreen state
    static HidTouchScreenState hidState;

    if (hidGetTouchScreenStates(&hidState, 1))
    {
        for (int i = 0; i < hidState.count; i++)
        {
            RawTouchState state;
            state.pressed    = true;
            state.fingerId   = hidState.touches[i].finger_id;
            state.position.x = hidState.touches[i].x / Application::windowScale;
            state.position.y = hidState.touches[i].y / Application::windowScale;
            states->push_back(state);
        }
#ifdef __SWITCH2__
        if (!states->empty())
        {
            this->padMouseMode = false;
        }
#endif
    }
}

void SwitchInputManager::sendRumbleInternal(HidVibrationDeviceHandle vibration_device[2], HidVibrationValue vibration_values[2], unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
    float low  = (float)lowFreqMotor / 0xFFFF;
    float high = (float)highFreqMotor / 0xFFFF;

    vibration_values[0].amp_low   = low;
    vibration_values[0].freq_low  = low * 50;
    vibration_values[0].amp_high  = high;
    vibration_values[0].freq_high = high * 100;

    vibration_values[1].amp_low   = low;
    vibration_values[1].freq_low  = low * 50;
    vibration_values[1].amp_high  = high;
    vibration_values[1].freq_high = high * 100;

    hidSendVibrationValues(vibration_device, vibration_values, 2);
}

void SwitchInputManager::sendRumbleInternal(HidVibrationDeviceHandle vibration_device[2], HidVibrationValue vibration_values[2],
    float lowFreq, float highFreq, float lowAmp, float highAmp)
{
    vibration_values[0].amp_low   = lowAmp;
    vibration_values[0].freq_low  = lowFreq;
    vibration_values[0].amp_high  = highAmp;
    vibration_values[0].freq_high = highFreq;

    vibration_values[1].amp_low   = lowAmp;
    vibration_values[1].freq_low  = lowFreq;
    vibration_values[1].amp_high  = highAmp;
    vibration_values[1].freq_high = highFreq;

    hidSendVibrationValues(vibration_device, vibration_values, 2);
}

void SwitchInputManager::sendRumbleRaw(float lowFreq, float highFreq, float lowAmp, float highAmp)
{
    padUpdate(&this->padStateHandheld);
    if (padStateHandheld.active_handheld)
    {
        sendRumbleInternal(m_vibration_device_handheld, m_vibration_values_handheld, lowFreq, highFreq, lowAmp, highAmp);
    }
    else
    {
        sendRumbleInternal(m_vibration_device_handles[0], m_vibration_values[0], lowFreq, highFreq, lowAmp, highAmp);
    }
}

void SwitchInputManager::sendRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
    padUpdate(&this->padStateHandheld);
    if (controller == 0 && padStateHandheld.active_handheld)
    {
        sendRumbleInternal(m_vibration_device_handheld, m_vibration_values_handheld, lowFreqMotor, highFreqMotor);
        return;
    }

    int localController = padStateHandheld.active_handheld ? controller - 1 : controller;
    sendRumbleInternal(m_vibration_device_handles[localController], m_vibration_values[localController], lowFreqMotor, highFreqMotor);
}

void SwitchInputManager::updateMouseStates(RawMouseState* state)
{
#ifdef __SWITCH2__
    if (this->padPointerReady && this->padPointer.active && this->padMouseMode)
    {
        // Map the fixed pointer area to layout points so the cursor,
        // hit-testing and gestures stay correct in every UI scale.
        const float scaleX      = (float)Application::ORIGINAL_WINDOW_WIDTH / (float)PAD_MOUSE_AREA_W;
        const float scaleY      = (float)Application::ORIGINAL_WINDOW_HEIGHT / (float)PAD_MOUSE_AREA_H;
        state->position         = Point((float)this->padPointer.x * scaleX, (float)this->padPointer.y * scaleY);
        state->offset           = Point((float)this->padPointer.delta_x * scaleX, (float)this->padPointer.delta_y * scaleY);
        state->scroll           = Point(0, (float)this->padPointer.wheel_delta_y);
        state->leftButton       = this->padMouseLeftDown;
        state->middleButton     = (padPointerGetButtons(&this->padPointer) & HidMouseButton_Middle) != 0;
        state->rightButton      = this->padMouseRightDown;
        lastCursorPosition      = state->position;
        this->padMouseLeftHeld  = state->leftButton;
        this->padMouseRightHeld = state->rightButton;
    }
    // Otherwise: report nothing; the pads keep L/R/ZL/ZR (seek).
#else
    if (currentMouseState.attributes & HidMouseAttribute_IsConnected)
    {
        state->position     = Point(currentMouseState.x, currentMouseState.y);
        state->offset       = Point(currentMouseState.delta_x, currentMouseState.delta_y);
        state->scroll       = Point(0, currentMouseState.wheel_delta_x);
        state->leftButton   = currentMouseState.buttons & HidMouseButton_Left;
        state->middleButton = currentMouseState.buttons & HidMouseButton_Middle;
        state->rightButton  = currentMouseState.buttons & HidMouseButton_Right;
        lastCursorPosition  = state->position;
    }
#endif
}

void SwitchInputManager::runloopStart()
{
    upToDateMouseState();
    handleMouse();
#ifdef __SWITCH2__
    handlePadPointer();
#endif
    handleKeyboard();
    handleControllerSensors();
}

#ifdef __SWITCH2__
void SwitchInputManager::handlePadPointer()
{
    if (!this->padPointerReady)
        return;

    const PadState* pad = this->padStateHandheld.active_handheld ? &this->padStateHandheld : &this->padsState[0];
    padPointerUpdate(&this->padPointer, pad);

    const Time now       = getCPUTimeUsec();
    const u32 buttons    = padPointerGetButtons(&this->padPointer);
    const bool leftDown  = (buttons & HidMouseButton_Left) != 0;
    const bool rightDown = (buttons & HidMouseButton_Right) != 0;
    const bool moved     = this->padPointer.delta_x != 0 || this->padPointer.delta_y != 0;
    const bool scrolled  = this->padPointer.wheel_delta_y != 0;

    if (!this->padMouseHasAnchor)
    {
        this->padMouseAnchorX   = this->padPointer.x;
        this->padMouseAnchorY   = this->padPointer.y;
        this->padMouseHasAnchor = true;
    }

    if (!this->padPointer.active)
    {
        if (this->padMouseMode)
        {
            this->padMouseMode    = false;
            this->padMouseAnchorX = this->padPointer.x;
            this->padMouseAnchorY = this->padPointer.y;
        }
        this->padMouseLeftDown  = false;
        this->padMouseRightDown = false;
        this->padMouseLeftHeld  = false;
        this->padMouseRightHeld = false;
        return;
    }

    const int netX      = this->padPointer.x - this->padMouseAnchorX;
    const int netY      = this->padPointer.y - this->padMouseAnchorY;
    const int distSq    = netX * netX + netY * netY;
    const bool netMoved = moved && distSq >= PAD_MOUSE_MOVE_THRESHOLD * PAD_MOUSE_MOVE_THRESHOLD;

    const bool gripOk = this->padMouseGripOk();

    if (this->padMouseMode)
    {
        if (!gripOk)
        {
            // Out of the mouse grip: back to controller mode immediately.
            this->exitPadMouseMode();
        }
        else
        {
            // While mouse mode is on, L/R/ZL/ZR belong to the pointer for the
            // whole mode (clicks, never pad actions). Real use keeps the mode
            // alive: net travel from the anchor (so sensor jitter while
            // resting never counts), wheel scroll, or a click.
            if (netMoved || scrolled || leftDown || rightDown)
            {
                this->padMouseLastActive = now;
                if (netMoved)
                {
                    this->padMouseAnchorX = this->padPointer.x;
                    this->padMouseAnchorY = this->padPointer.y;
                }
            }
            else if (now - this->padMouseLastActive >= PAD_MOUSE_IDLE_HIDE_USEC)
            {
                // Unused for 3 s: back to controller mode (cursor hides).
                this->exitPadMouseMode();
            }
        }
    }
    else if ((netMoved || scrolled) && gripOk)
    {
        this->padMouseMode       = true;
        this->padMouseLastActive = now;
        this->padMouseAnchorX    = this->padPointer.x;
        this->padMouseAnchorY    = this->padPointer.y;
    }

    this->padMouseLeftDown  = leftDown;
    this->padMouseRightDown = rightDown;
    this->padMouseLeftHeld  = leftDown;
    this->padMouseRightHeld = rightDown;
}
#endif

void SwitchInputManager::upToDateMouseState()
{
#ifndef __SWITCH2__
    hidGetMouseStates(&currentMouseState, 1);
#endif
}

void SwitchInputManager::handleMouse()
{
#ifdef __SWITCH2__
    // No ordinary mouse on Switch 2: USB mice are unsupported and the
    // Joy-Con pointer reports through updateMouseStates.
#else
    if (currentMouseState.attributes & HidMouseAttribute_IsConnected)
    {
        getMouseCusorOffsetChanged()->fire(Point(currentMouseState.delta_x, currentMouseState.delta_y));
        getMouseScrollOffsetChanged()->fire(Point(currentMouseState.wheel_delta_y, currentMouseState.wheel_delta_x));
    }
#endif
}

void SwitchInputManager::handleKeyboard()
{
    HidKeyboardState state;

    if (hidGetKeyboardStates(&state, 1))
    {
        for (int i = 0; i < 256; ++i)
        {
            auto is_pressed = (state.keys[i / 64] & (1ul << (i % 64))) != 0;
            if (m_hid_keyboard_state[i] != is_pressed)
            {
                m_hid_keyboard_state[i]            = is_pressed;
                const BrlsKeyboardScancode brlsKey = m_hid_brls_map[i];

                KeyState keyState { };
                keyState.key     = brlsKey;
                keyState.pressed = is_pressed;

                if (state.modifiers & HidKeyboardModifier_LeftAlt)
                    keyState.mods |= BRLS_KBD_MODIFIER_ALT;

                if (state.modifiers & HidKeyboardModifier_Control)
                    keyState.mods |= BRLS_KBD_MODIFIER_CTRL;

                if (state.modifiers & HidKeyboardModifier_Shift)
                    keyState.mods |= BRLS_KBD_MODIFIER_SHIFT;

                if (state.modifiers & HidKeyboardModifier_Gui)
                    keyState.mods |= BRLS_KBD_MODIFIER_META;

                getKeyboardKeyStateChanged()->fire(keyState);
            }
        }
    }
    else
    {
        Logger::debug("Keyboard failed!");
    }
}

void SwitchInputManager::handleControllerSensors()
{
    HidSixAxisSensorState sixaxis = { 0 };

    uint64_t style_set = padGetStyleSet(&padsState[0]);
    if (padStateHandheld.active_handheld)
        hidGetSixAxisSensorStates(this->m_six_axis_sensor_handle[0], &sixaxis, 1);
    else if (style_set & HidNpadStyleTag_NpadFullKey)
        hidGetSixAxisSensorStates(this->m_six_axis_sensor_handle[1], &sixaxis, 1);
    else if (style_set & HidNpadStyleTag_NpadJoyDual)
    {
        // For JoyDual, read from either the Left or Right Joy-Con depending on which is/are connected
        u64 attrib = padGetAttributes(&padsState[0]);
        if (attrib & HidNpadAttribute_IsLeftConnected)
            hidGetSixAxisSensorStates(this->m_six_axis_sensor_handle[2], &sixaxis, 1);
        else if (attrib & HidNpadAttribute_IsRightConnected)
            hidGetSixAxisSensorStates(this->m_six_axis_sensor_handle[3], &sixaxis, 1);
    }

    auto accelState = SensorEvent {
        0,
        SensorEventType::ACCEL,
        { -sixaxis.acceleration.x,
            -sixaxis.acceleration.z,
            sixaxis.acceleration.y },
        0
    };
    auto gyroState = SensorEvent {
        0,
        SensorEventType::GYRO,
        { static_cast<float>(sixaxis.angular_velocity.x * 2.0f * M_PI),
            static_cast<float>(sixaxis.angular_velocity.z * 2.0f * M_PI),
            static_cast<float>(-sixaxis.angular_velocity.y * 2.0f * M_PI) },
        0
    };
    getControllerSensorStateChanged()->fire(accelState);
    getControllerSensorStateChanged()->fire(gyroState);
    Application::setActiveEvent(true);
}

void SwitchInputManager::setPointerLock(bool lock)
{
    pointerLocked = lock;
}

void SwitchInputManager::drawCursor(NVGcontext* vg)
{
#ifdef __SWITCH2__
    if (this->padPointerReady && !pointerLocked && this->padMouseMode)
    {
        nvgBeginPath(vg);
        nvgCircle(vg, lastCursorPosition.x, lastCursorPosition.y, PAD_MOUSE_DOT_RADIUS);
        nvgFillColor(vg,
            nvgRGBA(255, 255, 255,
                (this->padMouseLeftHeld || this->padMouseRightHeld) ? PAD_MOUSE_DOT_ALPHA_CLICK
                                                                    : PAD_MOUSE_DOT_ALPHA));
        nvgFill(vg);
    }
#else
    initCursor(vg);
    if (!pointerLocked)
    {
        this->paint.xform[4] = lastCursorPosition.x;
        this->paint.xform[5] = lastCursorPosition.y;

        nvgBeginPath(vg);
        nvgRect(vg, lastCursorPosition.x, lastCursorPosition.y, this->cursorWidth, this->cursorHeight);
        nvgFillPaint(vg, this->paint);
        nvgFill(vg);
    }
#endif
}

void SwitchInputManager::initCursor(NVGcontext* vg)
{
    if (cursorInited)
        return;
    if (vg)
    {
#ifdef USE_LIBROMFS
        auto image          = romfs::get("img/sys/cursor.png");
        this->cursorTexture = nvgCreateImageMem(vg, NVG_IMAGE_NEAREST, (unsigned char*)image.data(), image.size());
#else
        this->cursorTexture = nvgCreateImage(vg, BRLS_ASSET("img/sys/cursor.png"), NVG_IMAGE_NEAREST);
#endif

        int width, height;
        nvgImageSize(vg, cursorTexture, &width, &height);
        float aspect       = (float)height / (float)width;
        this->cursorWidth  = 18;
        this->cursorHeight = 18 * aspect;

        this->paint        = nvgImagePattern(vg, 0, 0, this->cursorWidth, this->cursorHeight, 0, this->cursorTexture, 1.0f);
        this->cursorInited = true;
    }
}

static BrlsKeyboardScancode hidKey2BrlsKey(int key)
{
    if (KBD_A <= key && key <= KBD_Z)
    {
        return (BrlsKeyboardScancode)(key - KBD_A + BRLS_KBD_KEY_A);
    }
    if (KBD_1 <= key && key <= KBD_9)
    {
        return (BrlsKeyboardScancode)(key - KBD_1 + BRLS_KBD_KEY_1);
    }
    if (KBD_F1 <= key && key <= KBD_F12)
    {
        return (BrlsKeyboardScancode)(key - KBD_F1 + BRLS_KBD_KEY_F1);
    }
    if (KBD_KP1 <= key && key <= KBD_KP9)
    {
        return (BrlsKeyboardScancode)(key - KBD_KP1 + BRLS_KBD_KEY_KP_1);
    }

    switch (key)
    {
        case KBD_0:
            return BRLS_KBD_KEY_0;
        case KBD_SPACE:
            return BRLS_KBD_KEY_SPACE;
        case KBD_APOSTROPHE:
            return BRLS_KBD_KEY_APOSTROPHE;
        case KBD_COMMA:
            return BRLS_KBD_KEY_COMMA;
        case KBD_MINUS:
            return BRLS_KBD_KEY_MINUS;
        case KBD_DOT:
            return BRLS_KBD_KEY_PERIOD;
        case KBD_SLASH:
            return BRLS_KBD_KEY_SLASH;
        case KBD_SEMICOLON:
            return BRLS_KBD_KEY_SEMICOLON;
        case KBD_EQUAL:
            return BRLS_KBD_KEY_EQUAL;
        case KBD_LEFTBRACE:
            return BRLS_KBD_KEY_LEFT_BRACKET;
        case KBD_RIGHTBRACE:
            return BRLS_KBD_KEY_RIGHT_BRACKET;
        case KBD_BACKSLASH:
            return BRLS_KBD_KEY_BACKSLASH;
        case KBD_GRAVE:
            return BRLS_KBD_KEY_GRAVE_ACCENT;
        case KBD_ESC:
            return BRLS_KBD_KEY_ESCAPE;
        case KBD_ENTER:
            return BRLS_KBD_KEY_ENTER;
        case KBD_TAB:
            return BRLS_KBD_KEY_TAB;
        case KBD_BACKSPACE:
            return BRLS_KBD_KEY_BACKSPACE;
        case KBD_CAPSLOCK:
            return BRLS_KBD_KEY_CAPS_LOCK;
        case KBD_LEFTSHIFT:
            return BRLS_KBD_KEY_LEFT_SHIFT;
        case KBD_LEFTCTRL:
            return BRLS_KBD_KEY_LEFT_CONTROL;
        case KBD_LEFTALT:
            return BRLS_KBD_KEY_LEFT_ALT;
        case KBD_LEFTMETA:
            return BRLS_KBD_KEY_LEFT_SUPER;
        case KBD_RIGHTSHIFT:
            return BRLS_KBD_KEY_RIGHT_SHIFT;
        case KBD_RIGHTCTRL:
            return BRLS_KBD_KEY_RIGHT_CONTROL;
        case KBD_RIGHTALT:
            return BRLS_KBD_KEY_RIGHT_ALT;
        case KBD_RIGHTMETA:
            return BRLS_KBD_KEY_RIGHT_SUPER;
        case KBD_LEFT:
            return BRLS_KBD_KEY_LEFT;
        case KBD_RIGHT:
            return BRLS_KBD_KEY_RIGHT;
        case KBD_UP:
            return BRLS_KBD_KEY_UP;
        case KBD_DOWN:
            return BRLS_KBD_KEY_DOWN;

        case KBD_SYSRQ:
            return BRLS_KBD_KEY_PRINT_SCREEN;
        case KBD_SCROLLLOCK:
            return BRLS_KBD_KEY_SCROLL_LOCK;
        case KBD_PAUSE:
            return BRLS_KBD_KEY_PAUSE;
        case KBD_INSERT:
            return BRLS_KBD_KEY_INSERT;
        case KBD_HOME:
            return BRLS_KBD_KEY_HOME;
        case KBD_PAGEUP:
            return BRLS_KBD_KEY_PAGE_UP;
        case KBD_DELETE:
            return BRLS_KBD_KEY_DELETE;
        case KBD_END:
            return BRLS_KBD_KEY_END;
        case KBD_PAGEDOWN:
            return BRLS_KBD_KEY_PAGE_DOWN;

        case KBD_NUMLOCK:
            return BRLS_KBD_KEY_NUM_LOCK;
        case KBD_KPSLASH:
            return BRLS_KBD_KEY_KP_DIVIDE;
        case KBD_KPASTERISK:
            return BRLS_KBD_KEY_KP_MULTIPLY;
        case KBD_KPMINUS:
            return BRLS_KBD_KEY_KP_SUBTRACT;
        case KBD_KPPLUS:
            return BRLS_KBD_KEY_KP_ADD;
        case KBD_KPENTER:
            return BRLS_KBD_KEY_KP_ENTER;
        case KBD_KPDOT:
            return BRLS_KBD_KEY_KP_DECIMAL;
        case KBD_KP0:
            return BRLS_KBD_KEY_KP_0;

        case KBD_102ND:
            return BRLS_KBD_KEY_WORLD_1;

        // case KBD_HASHTILDE: return GLFW_HASHTILDE;
        default:
            return BRLS_KBD_KEY_UNKNOWN;
    }
}

void SwitchInputManager::hidInitializeKeyboardMap()
{
    m_hid_brls_map[KBD_NONE]             = BRLS_KBD_KEY_UNKNOWN;
    m_brls_hid_map[BRLS_KBD_KEY_UNKNOWN] = KBD_NONE;

    for (int i = 0; i < 256; ++i)
    {
        const auto brlsKey = hidKey2BrlsKey(i);
        if (brlsKey == BRLS_KBD_KEY_UNKNOWN)
            continue;
        m_hid_brls_map[i]       = brlsKey;
        m_brls_hid_map[brlsKey] = i;
    }
}

} // namespace brls
