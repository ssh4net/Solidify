#include "stdafx.h"
#include "ui.h"

#include "processing.h"

class MainWindow;

DropArea::DropArea() {
    QVBoxLayout* layout = new QVBoxLayout;
    QLabel* label = new QLabel("Drag & drop files here");

    QFont font = this->font();
    font.setPointSize(16);
    label->setFont(font);

    label->setAlignment(Qt::AlignCenter); // Set alignment to center

    layout->addWidget(label, 0, Qt::AlignCenter);

    setLayout(layout);
    setAcceptDrops(true);
}

void DropArea::dragEnterEvent(QDragEnterEvent* event) {
    event->acceptProposedAction();
}

void DropArea::dropEvent(QDropEvent* event) {
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        emit filesDropped(urls);  // Emit the signal when files are dropped
    }
}

MainWindow::MainWindow() {
    QVBoxLayout* layout = new QVBoxLayout;  // Create a vertical layout

    DropArea* dropArea = new DropArea;
    // set size of drop area to fill layout area
    dropArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    dropArea->setAutoFillBackground(true);  // Fill the background with the color below
    dropArea->setStyleSheet("border-radius: 3px; background-color: #E0E0E0; margin-bottom: 4px;");

    layout->addWidget(dropArea);  // Add the drop area to the layout

    // Progress bar
    progressBar = new QProgressBar;
    progressBar->setFixedHeight(20);
    progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    progressBar->setRange(0, 100);
    //progressBar->setValue(75);
    progressBar->setTextVisible(false);
    progressBar->setStyleSheet(
        "QProgressBar {border: 0px solid black; border-radius: 3px; background-color: white; color: black; margin-bottom: 4px;}"
        "QProgressBar::chunk {background-color: #05B8CC;}");
    layout->addWidget(progressBar);

    // Create a new QPlainTextEdit
    QPlainTextEdit* textOutput = new QPlainTextEdit;
    textOutput->setReadOnly(true);  // Make it read-only so users can't edit the text
    // set the size to fill width and 1/3 of the height
    textOutput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    textOutput->setFixedHeight(52);
    textOutput->setStyleSheet("border-radius: 3px; background-color: #E0E0E0; padding-left: 4px; padding-right: 4px;");
    // text size
    QFont font = this->font();
    font.setPointSize(8);
    font.setStyleHint(QFont::Monospace);
    textOutput->setFont(font);

    textOutput->setPlainText("Waiting for user inputs...");
    textOutput->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(textOutput);

    // widgets

    QWidget* centralWidget = new QWidget;
    centralWidget->setLayout(layout);  // Set the layout for the central widget
    setCentralWidget(centralWidget);  // Set the central widget of the MainWindow

    QMenuBar* menuBar = new QMenuBar;
    QMenu* f_menu = new QMenu("Files", menuBar);
    QMenu* s_menu = new QMenu("Settings", menuBar);
    //QMenu* r_menu = new QMenu("Reset", menuBar);
    //QAction* f_Restart = new QAction("Restart", r_menu);
    QAction* s_Settings = new QAction("Enable Console", s_menu);
    s_Settings->setCheckable(true);
    s_Settings->setChecked(false);
    QAction* f_Restart = new QAction("Restart", f_menu);
    QAction* f_Exit = new QAction("Exit", f_menu);
    //r_menu->addAction(f_Restart);
    s_menu->addAction(s_Settings);
    f_menu->addAction(f_Restart);
    f_menu->addSeparator();
    f_menu->addAction(f_Exit);
    menuBar->addMenu(f_menu);
    menuBar->addMenu(s_menu);
    setMenuBar(menuBar);

    setWindowFlags(Qt::WindowStaysOnTopHint);
    setWindowTitle("Solidify 1.30");
    setFixedSize(440, 440);
    
    // Add a flag to indicate whether the console has been allocated
    bool consoleAllocated = false;

    // Connect the signal from the drop area to the slot in the main window
    connect(dropArea, &DropArea::filesDropped, this, &MainWindow::startProcessing);

    // Connect the Settings action's triggered signal to a slot that toggles the console
    connect(s_Settings, &QAction::toggled, this, &MainWindow::toggleConsole);

    // Add new connection for updating the textOutput
    connect(this, &MainWindow::updateTextSignal, textOutput, &QPlainTextEdit::setPlainText);

    // Connect the Exit action's triggered signal to QApplication's quit slot
    connect(f_Exit, &QAction::triggered, qApp, &QApplication::quit);

    // Connect the Restart action's triggered signal to a slot that restarts the app
    connect(f_Restart, &QAction::triggered, this, &MainWindow::restartApp);
}

void MainWindow::startProcessing(QList<QUrl> urls) {
    // Move the processing to a separate thread
    QFuture<bool> future = QtConcurrent::run(doProcessing, urls, progressBar, this);

    processingWatcher.setFuture(future);

    connect(&processingWatcher, &QFutureWatcher<bool>::progressValueChanged, progressBar, &QProgressBar::setValue);
}

void MainWindow::toggleConsole(bool checked) {
    if (checked) {
        // Show console
        ShowWindow(GetConsoleWindow(), SW_SHOW);
    }
    else {
        // Hide console
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    }
}

void MainWindow::restartApp() {
    QStringList arguments;
    QProcess::startDetached(QApplication::applicationFilePath(), arguments);

    QApplication::quit();
}

void setPBarColor(QProgressBar* progressBar, const QColor& color) {
    QString style = QString(
        "QProgressBar {"
        "border: 0px solid black; border-radius: 3px; background-color: white; color: black;"
        "margin-bottom: 4px;"
        "}"
        "QProgressBar::chunk {background-color: %1;}"
    ).arg(color.name());

    progressBar->setStyleSheet(style);
}