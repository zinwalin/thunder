#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QLocale>

#include <global.h>

class QTranslator;

class NEXT_LIBRARY_EXPORT SettingsManager : public QObject {
    Q_OBJECT

public:
    static SettingsManager *instance();

    static void destroy();

    void registerProperty(const char *name, const QVariant &value);

    void setLanguage(const QLocale &language);

signals:
    void updated();

public slots:
    void loadSettings();
    void saveSettings();

private:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    SettingsManager();

    static SettingsManager *m_pInstance;

    QTranslator *m_Translator;
    QLocale m_Locale;

};

#endif // SETTINGSMANAGER_H
