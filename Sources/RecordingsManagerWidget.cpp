#include <fstream>
#include <QFile>

#include <QFileInfo>
#include <QDateTime>
#include <QDesktopServices>
#include <QDropEvent>
#include <QMimeData>

#include "ConverterWidget.h"
#include "RecordingsManagerWidget.h"

#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>

#include <SFML/Audio.hpp>


////////////// Initialize widget


RecordingsManagerWidget::RecordingsManagerWidget (QTabWidget* parent) : QWidget ()
{
    mainWindow = parent;

    setAcceptDrops (true);

    layout = new QGridLayout (this);
    layout->setAlignment (Qt::AlignBottom);
    layout->setSpacing (20);

    helpLabel = new QLabel (tr(
        "This is your recording manager : all your recordings are listed here !\n"
        "You can easily play, delete, move them from this tab\n"
        "and add new recordings by drag and drop from your file explorer or using button \"Add recordings\"."));
    helpLabel->setAlignment (Qt::AlignCenter);

    recordingsList = new QListWidget;
    recordingsList->setSortingEnabled (true);

    std::ifstream recordingsFile ("Recordings.pastouche");
    if (recordingsFile)
    {
        std::string recordingPath;

        while (getline (recordingsFile, recordingPath))
            if (QFile::exists (QString::fromStdString (recordingPath)))
                recordingsList->addItem (QString::fromStdString (recordingPath));
    }

    connect (this, SIGNAL (modifiedList ()), this, SLOT (updateUI ()));


    initActions ();

    initPlaybackTools ();


    layout->addWidget (helpLabel, 0, 0, 1, 2);
    layout->addWidget (recordingsList, 1, 0, 10, 1);

    layout->addWidget (bAddRecordings, 1, 1);
    layout->addWidget (bProperties, 2, 1);
    layout->addWidget (bShowInExplorer, 3, 1);
    layout->addWidget (bMove, 4, 1);
    layout->addWidget (bRename, 5, 1);
    layout->addWidget (bRemoveFromList, 6, 1);
    layout->addWidget (bClearRecordingsList, 7, 1);
    layout->addWidget (bDeleteRecording, 8, 1);
    layout->addWidget (bRemoveAllRecordings, 9, 1);
    layout->addWidget (bConvert, 10, 1);

    layout->addWidget (playbackTools, 11, 0, 1, 2);
}

void RecordingsManagerWidget::initActions ()
{
    bAddRecordings = new QPushButton (tr("&Add recordings"));
    bProperties = new QPushButton (tr("Pr&operties"));
    bShowInExplorer = new QPushButton (tr("Op&en folder"));
    bMove = new QPushButton (tr("&Move to..."));
    bRename = new QPushButton (tr("Re&name"));
    bRemoveFromList = new QPushButton (tr("&Remove from list"));
    bClearRecordingsList = new QPushButton (tr("&Clear list"));
    bDeleteRecording = new QPushButton (tr("&Delete"));
    bRemoveAllRecordings = new QPushButton (tr("Delete a&ll"));
    bConvert = new QPushButton (tr("Con&vert"));

    bProperties->setEnabled (false);
    bShowInExplorer->setEnabled (false);
    bMove->setEnabled (false);
    bRename->setEnabled (false);
    bDeleteRecording->setEnabled (false);
    bRemoveFromList->setEnabled (false);
    bConvert->setEnabled (false);

    if (recordingsList->count () == 0)
    {
        bClearRecordingsList->setEnabled (false);
        bRemoveAllRecordings->setEnabled (false);
    }

    connect (bAddRecordings, SIGNAL (clicked ()), this, SLOT (addRecordings ()));
    connect (bProperties, SIGNAL (clicked ()), this, SLOT (displayProperties ()));
    connect (bShowInExplorer, SIGNAL (clicked ()), this, SLOT (showInExplorer ()));
    connect (bMove, SIGNAL (clicked ()), this, SLOT (move ()));
    connect (bRename, SIGNAL (clicked ()), this, SLOT (rename ()));
    connect (bRemoveFromList, SIGNAL (clicked ()), this, SLOT (removeFromList ()));
    connect (bClearRecordingsList, SIGNAL (clicked ()), this, SLOT (clearRecordingsList ()));
    connect (bDeleteRecording, SIGNAL (clicked ()), this, SLOT (deleteRecording ()));
    connect (bRemoveAllRecordings, SIGNAL (clicked ()), this, SLOT (deleteAllRecordings ()));
    connect (bConvert, SIGNAL (clicked ()), this, SLOT (convert ()));

    connect (recordingsList, SIGNAL (itemDoubleClicked (QListWidgetItem*)), this, SLOT (play ()));
    connect (recordingsList, SIGNAL (currentTextChanged (const QString&)), this, SLOT (loadCurrentRecording (const QString&)));
}


void RecordingsManagerWidget::initPlaybackTools ()
{
    musicTimer = new QTimer (this);
    musicTimer->setTimerType (Qt::PreciseTimer);
    connect (musicTimer, SIGNAL (timeout ()), this, SLOT (updateSlider ()));

    playbackTools = new QWidget;
    playbackLayout = new QHBoxLayout (playbackTools);


    bPlay = new QPushButton (QIcon ("Start button.png"), "");
    bStop = new QPushButton (QIcon ("Stop button.png"), "");
    bStepBack = new QPushButton (QIcon ("Step back.png"), "");
    bStepForward = new QPushButton (QIcon ("Step forward.png"), "");

    bPlay->setToolTip (tr("Play"));
    bStop->setToolTip (tr("Stop"));
    bStepBack->setToolTip (tr("Step back (1/100)"));
    bStepForward->setToolTip (tr("Step forward (1/100)"));

    connect (bPlay, SIGNAL (clicked ()), this, SLOT (play ()));
    connect (bStop, SIGNAL (clicked ()), this, SLOT (stop ()));
    connect (bStepBack, SIGNAL (clicked ()), this, SLOT (stepBack ()));
    connect (bStepForward, SIGNAL (clicked ()), this, SLOT (stepForward ()));
    bPlay->setShortcut (QKeySequence (Qt::Key_Space));


    playbackBar = new DirectJumpSlider;
    connect (playbackBar, SIGNAL (sliderPressed ()), this, SLOT (onPressedSlider ()));
    connect (playbackBar, SIGNAL (directJumpOperated ()), this, SLOT (changePlayingOffset ()));
    connect (playbackBar, SIGNAL (valueChanged (int)), this, SLOT (onSliderValueChanged (int)));
    connect (playbackBar, SIGNAL (sliderReleased ()), this, SLOT (onReleasedSlider ()));

    playbackTimerLabel = new QLabel ("0:00");
    recordingDurationLabel = new QLabel ("0:00");


    playbackLayout->addWidget (bPlay);
    playbackLayout->addWidget (bStop);
    playbackLayout->addWidget (bStepBack);
    playbackLayout->addWidget (bStepForward);
    playbackLayout->addWidget (playbackTimerLabel);
    playbackLayout->addWidget (playbackBar);
    playbackLayout->addWidget (recordingDurationLabel);

    playbackTools->setEnabled (false);
}


void RecordingsManagerWidget::setConverter (ConverterWidget* newConverter)
{
    converter = newConverter;
}


RecordingsManagerWidget::~RecordingsManagerWidget ()
{
    std::ofstream recordingsFile ("Recordings.pastouche");

    for (int i = 0 ; i != recordingsList->count () ; i++)
        recordingsFile<<recordingsList->item (i)->text ().toStdString ()<<"\n";
}


////////////// UI update slots


void RecordingsManagerWidget::loadCurrentRecording (const QString& currentFileName)
{
    bProperties->setEnabled (true);
    bShowInExplorer->setEnabled (true);
    bMove->setEnabled (true);
    bRename->setEnabled (true);
    bDeleteRecording->setEnabled (true);
    bRemoveFromList->setEnabled (true);
    bConvert->setEnabled (true);

    playbackTools->setEnabled (true);
    bStop->setEnabled (false);
    bStepBack->setEnabled (false);
    playbackBar->setValue (0);


    if (currentFileName.isEmpty ())
        recordingDurationLabel->setText ("0:00");

    else if (recording.openFromFile (std::string (currentFileName.toLocal8Bit ())))
    {
        recording.play ();
        recording.pause ();
        recording.setPlayingOffset (sf::Time::Zero);

        bPlay->setIcon (QIcon ("Start button.png"));
        bPlay->setToolTip (tr("Play"));
        playbackBar->setRange (0, recording.getDuration ().asMilliseconds ());

        unsigned int minutes = recording.getDuration ().asMilliseconds () / 1000 / 60;
        unsigned int seconds = recording.getDuration ().asMilliseconds () / 1000 % 60;

        recordingDurationLabel->setText (QString::number (minutes) + ":" + (seconds < 10 ? "0" : "") + QString::number (seconds));
    }
    else
    {
        if (QMessageBox::question (this, tr("Missing file"), tr("This file doesn't exists anymore,\ndo you want to remove it from the list ?")) == QMessageBox::Yes)
        {
            QFile::remove (currentFileName);
            removeCurrentFromList ();
        }
    }
}

void RecordingsManagerWidget::updateUI ()
{
    if (recordingsList->count () == 0)
    {
        bProperties->setEnabled (false);
        bShowInExplorer->setEnabled (false);
        bMove->setEnabled (false);
        bRename->setEnabled (false);
        bDeleteRecording->setEnabled (false);
        bRemoveFromList->setEnabled (false);
        bConvert->setEnabled (false);

        bClearRecordingsList->setEnabled (false);
        bRemoveAllRecordings->setEnabled (false);

        playbackTools->setEnabled (false);
    }
    else
    {
        bClearRecordingsList->setEnabled (true);
        bRemoveAllRecordings->setEnabled (true);
    }
}


////////////// Playback slots


void RecordingsManagerWidget::updateSlider ()
{
    playbackBar->setValue (recording.getPlayingOffset ().asMilliseconds ());


    if (recording.getStatus () == sf::SoundSource::Stopped)
        stop ();
}


void RecordingsManagerWidget::onPressedSlider ()
{
    if (recording.getStatus () == sf::SoundSource::Playing)
    {
        oldStatus = sf::SoundSource::Playing;

        recording.pause ();
        musicTimer->stop ();
    }
    else
        oldStatus = recording.getStatus ();
}

void RecordingsManagerWidget::onSliderValueChanged (int value)
{
    unsigned int minutes = value / 1000 / 60;
    unsigned int seconds = value / 1000 % 60;

    playbackTimerLabel->setText (QString::number (minutes) + ":" + (seconds < 10 ? "0" : "") + QString::number (seconds));
}

void RecordingsManagerWidget::changePlayingOffset ()
{
    recording.setPlayingOffset (sf::milliseconds (playbackBar->value ()));
}

void RecordingsManagerWidget::onReleasedSlider ()
{
    recording.setPlayingOffset (sf::milliseconds (playbackBar->value ()));

    if (oldStatus == sf::SoundSource::Playing)
    {
        recording.play ();
        musicTimer->start (20);
    }
}


void RecordingsManagerWidget::play ()
{
    if (recording.getStatus () == sf::SoundSource::Playing)
    {
        bPlay->setIcon (QIcon ("Start button.png"));
        bPlay->setToolTip (tr("Play"));

        recording.pause ();
        musicTimer->stop ();
    }
    else
    {
        bPlay->setIcon (QIcon ("Pause button.png"));
        bPlay->setToolTip (tr("Pause"));
        bStop->setEnabled (true);
        bStepBack->setEnabled (true);

        recording.play ();
        musicTimer->start (20);
    }
}

void RecordingsManagerWidget::stop ()
{
    musicTimer->stop ();
    recording.pause ();
    recording.setPlayingOffset (sf::seconds (0));

    playbackBar->setValue (0);
    bPlay->setIcon (QIcon ("Start button.png"));
    bPlay->setToolTip (tr("Play"));
    bStop->setEnabled (false);
    bStepBack->setEnabled (false);
}

void RecordingsManagerWidget::stepBack ()
{
    sf::Time step (sf::milliseconds (recording.getDuration ().asMilliseconds () / 100));

    if (recording.getPlayingOffset () - step > sf::seconds (0))
        recording.setPlayingOffset (recording.getPlayingOffset () - step);

    else
        stop ();

    updateSlider ();
}

void RecordingsManagerWidget::stepForward ()
{
    sf::Time step (sf::milliseconds (recording.getDuration ().asMilliseconds () / 100));

    if (recording.getDuration () > recording.getPlayingOffset () + step)
    {
        recording.setPlayingOffset (recording.getPlayingOffset () + step);
        bStop->setEnabled (true);
        bStepBack->setEnabled (true);
    }
    else
        stop ();

    updateSlider ();
}


////////////// Files manager slots


void RecordingsManagerWidget::displayProperties ()
{
    QString fileName (recordingsList->currentItem ()->text ());

    sf::Music music;
    if (!music.openFromFile (std::string (fileName.toLocal8Bit ())))
    {
        if (QMessageBox::question (this, tr("Ooooops..."), tr("Impossible to load this file,\nit must be corrupted !\nDo you want to delete it ?")) == QMessageBox::Yes)
        {
            QFile::remove (recordingsList->currentItem ()->text ());
            removeCurrentFromList ();
        }
    }
    else
    {
        QFile file (fileName);

        QString properties (tr("Name : ") + fileName +
                            tr("\nRecorded on : ") + QFileInfo (file).birthTime ().date ().toString (tr("MM/dd/yyyy")) +
                            tr("\n\nSample rate : ") + QString::number (music.getSampleRate ()) + " Hz" +
                            tr("\nChannels : ") + QString::number (music.getChannelCount ()) +
                            tr("\nSize : "));


        if (file.size () >= 1048576)
            properties += QString::number (file.size () / 1048576) + tr(" MB\nDuration : ");

        else
            properties += QString::number (file.size () / 1024) + tr(" KB\nDuration : ");


        unsigned int minutes = int (music.getDuration ().asSeconds ()) / 60;
        unsigned int seconds = int (music.getDuration ().asSeconds ()) % 60;

        if (!minutes)
            properties += seconds == 1 ? tr("1 second") : tr("%n seconds", "", seconds);

        else if (seconds)
            properties += (minutes == 1 ? tr("1 minute and ") : tr("%n minutes and ", "", minutes)) +
                          (seconds == 1 ? tr("1 second") : tr("%n seconds", "", seconds));

        else
            properties += minutes == 1 ? tr("1 minute") : tr("%n minutes", "", minutes);

        QMessageBox::information (this, tr("File properties"), properties);
    }
}

void RecordingsManagerWidget::showInExplorer ()
{
    QString fileName (recordingsList->currentItem ()->text ());
    QString fileFolder;

    int lastSlashIndex = fileName.lastIndexOf ("/") + 1;

    for (int i = 0 ; i != lastSlashIndex ; i++)
        fileFolder += fileName.at (i);


    QDesktopServices::openUrl (QUrl ("file:///" + fileFolder, QUrl::TolerantMode));
}

void RecordingsManagerWidget::move ()
{
    QString fileName = recordingsList->currentItem ()->text ();

    QString dest = QFileDialog::getExistingDirectory (this, tr("Select destination folder"), QFileInfo (fileName).dir ().path (), QFileDialog::DontUseNativeDialog | QFileDialog::ShowDirsOnly);

    QString destFileName = QString (dest + "/" + QFileInfo (fileName).fileName ()).replace ("//", "/");


    if (dest.isEmpty () || destFileName == fileName)
        return;

    if (QFile::exists (destFileName))
    {
        if (QMessageBox::question (this, tr("Confirmation"), tr("A file with the same name already exists there,\ndo you want to replace it ?")) != QMessageBox::Yes)
            return;

        QFile::remove (destFileName);
    }

    QFile::copy (fileName, destFileName);


    recordingsList->currentItem ()->setText (destFileName);
    emit recordingsList->currentTextChanged (destFileName);
    QFile::remove (fileName);


    QMessageBox::information (this, tr("Operation successful !"), tr("File ") + fileName + tr("\nmoved to ") + dest);
}

void RecordingsManagerWidget::rename ()
{
    QString fileName = recordingsList->currentItem ()->text ();

    bool ok;
    QString newFileName = QInputDialog::getText (this, tr("Rename file"), tr("Please input the new file name without suffix :"), QLineEdit::Normal, QFileInfo (fileName).baseName (), &ok);
    newFileName = newFileName.remove (QRegExp ("^ +")).remove (QRegExp (" +$"));

    if (!ok)
        return;


    newFileName = QString (QFileInfo (fileName).dir ().path () + "/" + newFileName + "." + QFileInfo (fileName).suffix ()).replace ("//", "/");

    if (newFileName == fileName)
        return;


    stop ();
    recording.openFromFile ("");

    if (!QFile::rename (fileName, newFileName))
        QMessageBox::critical (this, tr("Error"), tr("Impossible to rename the file\n") + newFileName);

    else
    {
        recordingsList->currentItem ()->setText (newFileName);
        emit recordingsList->currentTextChanged (newFileName);

        QMessageBox::information (this, tr("Operation successful !"), tr("Your recording was renamed\n") + newFileName);
    }
}


void RecordingsManagerWidget::addRecordings ()
{
    QStringList newRecordings = QFileDialog::getOpenFileNames (this, tr("Select files to add to your recordings"), "", tr("Audio files (*.ogg *.flac *.wav)"), nullptr, QFileDialog::DontUseNativeDialog);

    for (short int i = 0 ; i != newRecordings.length () ; i++)
        addRecording (newRecordings.at (i));
}

void RecordingsManagerWidget::removeFromList ()
{
    removeCurrentFromList ();
}

void RecordingsManagerWidget::clearRecordingsList ()
{
    if (QMessageBox::question (this, tr("Confirmation"), tr("Do you really want to clear the list ?\nThis won't remove your recordings.")) == QMessageBox::Yes)
    {
        recordingsList->clear ();

        updateUI ();
    }
}


void RecordingsManagerWidget::deleteRecording ()
{
    QString fileName (recordingsList->currentItem ()->text ());

    if (QMessageBox::question (this, tr("Confirmation"), tr("Do you really want to permanently delete\n") + fileName + " ?") == QMessageBox::Yes)
    {
        stop ();
        recording.openFromFile ("");

        if (!QFile::remove (fileName))
            QMessageBox::critical (this, tr("Error"), tr("Impossible to delete this file,\nyou must already did it."));

        removeCurrentFromList ();
    }
}

void RecordingsManagerWidget::deleteAllRecordings ()
{
    if (QMessageBox::question (this, tr("Confirmation"), tr("Do you really want to remove all your recordings ?\nThis action cannot be undone.")) == QMessageBox::Yes)
    {
        recordingsList->setCurrentRow (0);

        while (recordingsList->count ())
        {
            recording.openFromFile ("");

            if (!QFile::remove (recordingsList->currentItem ()->text ()))
                QMessageBox::critical (this, tr("Error"), tr("Impossible to delete ") + recordingsList->currentItem ()->text () + tr("\nYou must already did it."));

            recordingsList->takeItem (0);
        }

        updateUI ();
    }
}


void RecordingsManagerWidget::convert ()
{
    converter->addFile (recordingsList->currentItem ()->text ());

    mainWindow->setCurrentIndex (2);
}


////////////// Others


void RecordingsManagerWidget::dragEnterEvent (QDragEnterEvent* event)
{
    if (event->mimeData ()->hasUrls ())
        event->acceptProposedAction ();

    else
        event->ignore ();
}

void RecordingsManagerWidget::dropEvent (QDropEvent* event)
{
    if (event->mimeData ()->hasUrls ())
    {
        QList<QUrl> droppedFiles (event->mimeData ()->urls ());
        QString currentFile;

        for (short int i = 0 ; i != droppedFiles.length () ; i++)
        {
            currentFile = droppedFiles.at (i).toString ().remove ("file:///");

            if (QStringList ({"ogg", "flac", "wav"}).contains (QFileInfo (currentFile).suffix ().toLower ()))
                addRecording (currentFile);

            else
                QMessageBox::warning (this, tr("Error"), tr("Impossible to import ") + currentFile + tr(",\nyou can only import OGG, FLAC and WAV files !"));
        }

        updateUI ();
        event->acceptProposedAction ();
    }
    else
        event->ignore ();
}


void RecordingsManagerWidget::addRecording (const QString& fileName)
{
    if (recordingsList->findItems (fileName, Qt::MatchExactly).length () == 0)
        recordingsList->addItem (fileName);

    emit modifiedList ();
}

void RecordingsManagerWidget::removeCurrentFromList ()
{
    recordingsList->takeItem (recordingsList->currentRow ());

    emit modifiedList ();
}
