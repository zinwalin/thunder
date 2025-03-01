#ifndef UNDOMANAGER_H
#define UNDOMANAGER_H

#include <QUndoCommand>

class UndoCommand : public QUndoCommand {
public:
    explicit UndoCommand(const QString &text, QObject *editor = nullptr, QUndoCommand *parent = nullptr) :
        QUndoCommand(text, parent),
        m_editor(editor) {

    }

    QObject *editor() const {
        return m_editor;
    }

private:
    QObject *m_editor;
};

class UndoManager : public QUndoStack {
    Q_OBJECT

public:
    static UndoManager *instance();

    static void destroy();

    const UndoCommand *lastCommand(const QObject *editor) const;

private:
    UndoManager() {}
    ~UndoManager() {}

    static UndoManager *m_pInstance;
};

#endif // UNDOMANAGER_H
