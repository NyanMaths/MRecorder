#include <QFontDatabase>
#include <QTranslator>
#include <QScreen>

#include <fstream>

#include "Application.h"


Application::Application () : QTabWidget ()
{
    QFontDatabase::addApplicationFont ("Ubuntu.ttf");
    qApp->setFont (QFont ("Ubuntu"));


    std::string lang ("en");

    std::ifstream languageFile ("UI Options.pastouche");
    if (languageFile)
        getline (languageFile, lang);

    translator = new QTranslator;
    translator->load("mrecorder_" + QString::fromStdString (lang));
    qApp->installTranslator (translator);

    messageBoxesTranslator = new QTranslator;
    messageBoxesTranslator->load ("qtbase_" + QString::fromStdString (lang));
    qApp->installTranslator (messageBoxesTranslator);


    setWindowIcon (QIcon ("Window Icon.png"));

    optionsTab = new OptionsWidget;
    recordingsTab = new RecordingsManagerWidget (this);
    converterTab = new ConverterWidget (recordingsTab);
    recorderTab = new RecorderWidget (this, recordingsTab);

    recordingsTab->setConverter (converterTab);

    addTab (recorderTab, tr("Audio recorder"));
    addTab (recordingsTab, tr("My recordings"));
    addTab (converterTab, tr("File converter"));
    addTab (optionsTab, tr("About and settings"));

    setTabIcon (0, QIcon ("Recorder Image.png"));
    setTabIcon (1, QIcon ("Start button.png"));
    setTabIcon (2, QIcon ("Convert.png"));
    setTabIcon (3, QIcon ("Settings.png"));


    show ();

    setFixedSize (width (), height () + 200);

    QSize screenSize = QGuiApplication::screens ().at (0)->size ();
    move (screenSize.width () / 2 - width () / 2, screenSize.height () / 2 - height () / 2 - 20);
}


void Application::closeEvent (QCloseEvent* event)
{
    if (recorderTab->beforeExit ())
        event->accept ();

    else
        event->ignore ();
}
