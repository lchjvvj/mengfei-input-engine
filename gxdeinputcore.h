signals:
    void preeditChanged(const QString &text, int cursor);
    void candidateListChanged(const QStringList &list, int sel);
    void textCommitted(const QString &text);
    void keyRawEvent(uint32_t keycode);
    void keyUp();
    void keyDown();
    void keyPageUp();
    void keyPageDown();
    void keyNumber(int num);
