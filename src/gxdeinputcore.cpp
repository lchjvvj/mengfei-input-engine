bool GXDEInputCore::handleKeyEvent(uint32_t keycode, bool pressed)
{
    if (!m_ctx.active || !pressed)
        return false;

    // 这里是简化映射，真实环境需要使用libinput的keysym转换
    switch(keycode)
    {
        case 106: // Down
            emit keyDown();
            return true;
        case 103: // Up
            emit keyUp();
            return true;
        case 104: // PageUp
            emit keyPageUp();
            return true;
        case 109: // PageDown
            emit keyPageDown();
            return true;
        case 2: case 3: case 4: case 5: case 6:
        case 7: case 8: case 9: case 10: // 主键盘1~9
            emit keyNumber(keycode - 2);
            return true;
        default:
            // 普通字符，传给拼音引擎
            emit keyRawEvent(keycode);
            return true;
    }
}
