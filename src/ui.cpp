/*
 * Solidify (Push Pull) algorithm implementation using OpenImageIO
 * Copyright (c) 2022 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "ui.h"

#include "processing.h"
#include "settings.h"

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
    layout->addWidget(dropArea);

    // Progress bar
    progressBar = new QProgressBar;
    progressBar->setFixedHeight(20);
    progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
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

    // file menu
    QAction* f_Restart = new QAction("Restart", f_menu);
    QAction* f_Exit = new QAction("Exit", f_menu);
    
    // settings menu
    sld_enable = new QAction("Solidify", s_menu);
    sld_enable->setCheckable(true);
    sld_enable->setChecked(settings.isSolidify);
    // preserve alpha
    alf_enable = new QAction("With Alpha", s_menu);
    alf_enable->setCheckable(true);
    alf_enable->setChecked(settings.expAlpha);

    QAction* con_enable = new QAction("Enable Console", s_menu);
    con_enable->setCheckable(true);
    con_enable->setChecked(settings.conEnable);

    // Submenu
    QMenu* nm_submenu = new QMenu("Normalize", s_menu);
    QMenu* rng_submenu = new QMenu("Floats type", s_menu);
    QMenu* fmt_submenu = new QMenu("Formats", s_menu);
    QMenu* bit_submenu = new QMenu("Bits Depth", s_menu);
    QMenu* raw_submenu = new QMenu("Camera Raw", s_menu);
    QActionGroup* NormGroup = new QActionGroup(nm_submenu);
    QActionGroup* RangeGroup = new QActionGroup(rng_submenu);
    QActionGroup* FrmtGroup = new QActionGroup(fmt_submenu);
    QActionGroup* BitsGroup = new QActionGroup(bit_submenu);
    QActionGroup* RawGroup = new QActionGroup(raw_submenu);

    auto createAction = [](const QString& title, QActionGroup* group, QMenu* menu, bool checkable = true, bool checked = false) {
        QAction* action = new QAction(title, menu);
        action->setCheckable(checkable);
        action->setChecked(checked);
        group->addAction(action);
        menu->addAction(action);
        return action;
    };
    // Normals
    nrm_Dis = createAction("Disable", NormGroup, nm_submenu);
    nrm_Smrt = createAction("Smart", NormGroup, nm_submenu, true, true);
    nrm_Force = createAction("Force", NormGroup, nm_submenu);

    // Range
    rng_Unsg = createAction("Unsigned", RangeGroup, rng_submenu, true, true);
    rng_Sign = createAction("Signed", RangeGroup, rng_submenu);
    rng_SU = createAction("Signed <> Unsigned", RangeGroup, rng_submenu);
    rng_US = createAction("Unsigned <> Signed", RangeGroup, rng_submenu);

    // File Formats
    frmt_Org = createAction("Original", FrmtGroup, fmt_submenu, true, true);
    frmt_Tif = createAction("TIFF", FrmtGroup, fmt_submenu);
    frmt_Exr = createAction("OpenEXR", FrmtGroup, fmt_submenu);
    frmt_Png = createAction("PNG", FrmtGroup, fmt_submenu);
    frmt_Jpg = createAction("JPEG", FrmtGroup, fmt_submenu);
    frmt_Jp2 = createAction("JPEG-2000", FrmtGroup, fmt_submenu);
    frmt_Ppm = createAction("PPM", FrmtGroup, fmt_submenu);

    // Bits Depth
    bit_orig = createAction("Original", BitsGroup, bit_submenu, true, true);
    bit_uint8 = createAction("8 bits int", BitsGroup, bit_submenu);
    bit_uint16 = createAction("16 bits int", BitsGroup, bit_submenu);
    bit_uint32 = createAction("32 bits int", BitsGroup, bit_submenu);
    bit_uint64 = createAction("64 bits int", BitsGroup, bit_submenu);
    bit_flt16 = createAction("16 bits float", BitsGroup, bit_submenu);
    bit_flt32 = createAction("32 bits float", BitsGroup, bit_submenu);
	bit_flt64 = createAction("64 bits float", BitsGroup, bit_submenu);

    // camera raw rotation
    raw_rot_A = createAction("Auto EXIF", RawGroup, raw_submenu, true, true);
    raw_rot_0 = createAction("0 Horizontal", RawGroup, raw_submenu);
    raw_rot_90 = createAction("-90 Vertical", RawGroup, raw_submenu);
    raw_rot_270 = createAction("+90 Vertical", RawGroup, raw_submenu);
    raw_rot_180 = createAction("180 Horizontal", RawGroup, raw_submenu);

    s_menu->addAction(sld_enable);
    s_menu->addSeparator();
    s_menu->addAction(alf_enable);
    s_menu->addSeparator();
    s_menu->addMenu(nm_submenu);
    s_menu->addMenu(rng_submenu);
    s_menu->addSeparator();
    s_menu->addMenu(fmt_submenu);
    s_menu->addMenu(bit_submenu);
    s_menu->addSeparator();
    s_menu->addMenu(raw_submenu);
    s_menu->addSeparator();
    s_menu->addAction(con_enable);
    //
    f_menu->addAction(f_Restart);
    f_menu->addSeparator();
    f_menu->addAction(f_Exit);
    menuBar->addMenu(f_menu);
    menuBar->addMenu(s_menu);
    setMenuBar(menuBar);

    setWindowFlags(Qt::WindowStaysOnTopHint);
    setWindowTitle(QString("Solidify %1.%2").arg(VERSION_MAJOR).arg(VERSION_MINOR));
    setFixedSize(500, 500);

    // Connect the signal from the drop area to the slot in the main window
    connect(dropArea, &DropArea::filesDropped, this, &MainWindow::startProcessing);

    // Connect the Settings action's triggered signal

    connect(sld_enable, &QAction::toggled, this, &MainWindow::sldfSettings);
    connect(alf_enable, &QAction::toggled, this, &MainWindow::alfSettings);

    QList<QAction*> norm_act = { nrm_Dis, nrm_Smrt, nrm_Force };
    for (QAction* action : norm_act) {
		connect(action, &QAction::triggered, this, &MainWindow::normSettings);
	}
    QList<QAction*> rng_act = { rng_Unsg, rng_Sign, rng_US, rng_SU };
    for (QAction* action : rng_act) {
        connect(action, &QAction::triggered, this, &MainWindow::rngSettings);
    }
    QList<QAction*> format_act = { frmt_Org, frmt_Tif, frmt_Exr, frmt_Png, frmt_Jpg, frmt_Jp2, frmt_Ppm };
    for (QAction* action : format_act) {
        connect(action, &QAction::triggered, this, &MainWindow::frmtSettings);
    }
    QList<QAction*> bits_act = { bit_orig, bit_uint8, bit_uint16, bit_uint32, bit_uint64, bit_flt16, bit_flt32, bit_flt64 };
    for (QAction* action : bits_act) {
		connect(action, &QAction::triggered, this, &MainWindow::bitSettings);
	}
    QList<QAction*> raw_act = { raw_rot_A, raw_rot_0, raw_rot_90, raw_rot_270, raw_rot_180 };
    for (QAction* action : raw_act) {
		connect(action, &QAction::triggered, this, &MainWindow::rawSettings);
	}

    connect(con_enable, &QAction::toggled, this, &MainWindow::toggleConsole);
    // Add new connection for updating the textOutput
    connect(this, &MainWindow::updateTextSignal, textOutput, &QPlainTextEdit::setPlainText);

    // Connect the Exit action's triggered signal to QApplication's quit slot
    connect(f_Exit, &QAction::triggered, qApp, &QApplication::quit);

    // Connect the Restart action's triggered signal to a slot that restarts the app
    connect(f_Restart, &QAction::triggered, this, &MainWindow::restartApp);
}

void MainWindow::bitSettings() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action == bit_orig) {
        settings.bitDepth = -1;
        emit updateTextSignal("Bit depth will remain original");
        qDebug() << "Bit depth will remain";
    }
    else if (action == bit_uint8) {
        settings.bitDepth = 0;
        emit updateTextSignal("Bit depth set to 8 bits int");
        qDebug() << "Bit depth set to 8 bits int";
    }
    else if (action == bit_uint16) {
		settings.bitDepth = 1;
		emit updateTextSignal("Bit depth set to 16 bits int");
		qDebug() << "Bit depth set to 16 bits int";
	}
    else if (action == bit_uint32) {
		settings.bitDepth = 2;
		emit updateTextSignal("Bit depth set to 32 bits int");
		qDebug() << "Bit depth set to 32 bits int";
	}
    else if (action == bit_uint64) {
		settings.bitDepth = 3;
		emit updateTextSignal("Bit depth set to 64 bits int");
		qDebug() << "Bit depth set to 64 bits int";
	}
    else if (action == bit_flt16) {
		settings.bitDepth = 4;
		emit updateTextSignal("Bit depth set to 16 bits float");
		qDebug() << "Bit depth set to 16 bits float";
	}
    else if (action == bit_flt32) {
		settings.bitDepth = 5;
		emit updateTextSignal("Bit depth set to 32 bits float");
		qDebug() << "Bit depth set to 32 bits float";
	}
    else if (action == bit_flt64) {
		settings.bitDepth = 6;
		emit updateTextSignal("Bit depth set to 64 bits float");
		qDebug() << "Bit depth set to 64 bits float";
	}
}

void MainWindow::frmtSettings() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action == frmt_Org) {
		settings.fileFormat = -1;
		emit updateTextSignal("File format will remain original");
		qDebug() << "File format will remain original";
	}
    else if (action == frmt_Tif) {
		settings.fileFormat = 0;
		emit updateTextSignal("File format set to TIFF");
		qDebug() << "File format set to TIFF";
	}
    else if (action == frmt_Exr) {
		settings.fileFormat = 1;
		emit updateTextSignal("File format set to OpenEXR");
		qDebug() << "File format set to OpenEXR";
	}
    else if (action == frmt_Png) {
		settings.fileFormat = 2;
		emit updateTextSignal("File format set to PNG");
		qDebug() << "File format set to PNG";
	}
    else if (action == frmt_Jpg) {
		settings.fileFormat = 3;
		emit updateTextSignal("File format set to JPEG");
		qDebug() << "File format set to JPEG";
	}
    else if (action == frmt_Jp2) {
		settings.fileFormat = 4;
		emit updateTextSignal("File format set to JPEG-2000");
		qDebug() << "File format set to JPEG-2000";
	}
    else if (action == frmt_Ppm) {
        settings.fileFormat = 5;
        emit updateTextSignal("File format set to PPM");
        qDebug() << "File format set to PPM";
    }
}

void MainWindow::normSettings() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action == nrm_Dis) {
        settings.normMode = 0;
        emit updateTextSignal("Normalizing disabled");
        qDebug() << "Normalizing disabled";
    }
    else if (action == nrm_Smrt) {
        settings.normMode = 1;
        emit updateTextSignal("Smart Normalizing - only normal maps" );
        qDebug() << "Smart Normalizing - only normal maps (normals/tangent/object/world in name)";
    }
    else if (action == nrm_Force) {
        settings.normMode = 2;
        emit updateTextSignal("Force Normalizing - all maps are Normals");
        qDebug() << "Force Normalizing - all maps are normals regardless of name";
    }
}

void MainWindow::sldfSettings(bool checked) {
    settings.isSolidify = sld_enable->isChecked();
    if (checked) {
        alf_enable->setChecked(false);
        emit updateTextSignal("Solidify Enabled");
        qDebug() << "Solidify Enabled";
        emit updateTextSignal("Export Alpha Disabled");
        qDebug() << "Export Alpha Disabled";
    }
    else {
        nrm_Smrt->setChecked(false);
        nrm_Dis->setChecked(true);
        settings.normMode = 0;
        emit updateTextSignal("Solidify Disabled");
        emit updateTextSignal("Normalizing disabled");
        qDebug() << "Normalizing disabled";
        qDebug() << "Solidify Disabled";
    }
}

void MainWindow::alfSettings(bool checked) {
    settings.expAlpha = alf_enable->isChecked();
    if (checked) {
		emit updateTextSignal("Export Alpha Enabled");
		qDebug() << "Export Alpha Enabled";
	}
    else {
		emit updateTextSignal("Export Alpha Disabled");
		qDebug() << "Export Alpha Disabled";
	}
}

void MainWindow::rngSettings() {
	QAction* action = qobject_cast<QAction*>(sender());
    if (action == rng_Unsg) {
        settings.rangeMode = 0;
		emit updateTextSignal("Unsigned floats");
        qDebug() << "32/16 bit float Normals are in [0.0 ~ 1.0] range";
	}
    else if (action == rng_Sign) {
        settings.rangeMode = 1;
		emit updateTextSignal("Signed floats");
        qDebug() << "32/16 bit floats Normals are in [-1.0 ~ 1.0] range";
	}
    else if (action == rng_SU) {
		settings.rangeMode = 2;
        emit updateTextSignal("Signed <> Unsigned");
        qDebug() << "32/16 bit floats Normals are in [-1.0 ~ 1.0] range converted to [0.0 ~ 1.0]";
    }
    else if (action == rng_US) {
        settings.rangeMode = 3;
        emit updateTextSignal("Unsigned <> Signed");
        qDebug() << "32/16 bit floats Normals are in [0.0 ~ 1.0] range converted to [-1.0 ~ 1.0]";
    }
}

void MainWindow::rawSettings() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action == raw_rot_A) {
        settings.rawRot = settings.raw_rot[0];
        emit updateTextSignal("Camera Raw rotation - Auto");
        qDebug() << "Camera Raw rotation set to EXIF Auto";
    }
    else if (action == raw_rot_0) {
        settings.rawRot = settings.raw_rot[1];
        emit updateTextSignal("Camera Raw rotation - 0 (Unrotate)");
        qDebug() << "Camera Raw rotation set to 0 degree - Unrotatate/Horizontal";
    }
    else if (action == raw_rot_90) {
		settings.rawRot = settings.raw_rot[3];
        emit updateTextSignal("Camera Raw rotation - 90 CW (Vertical)");
		qDebug() << "Camera Raw rotation set to 90 degree CW - Vertical";
	}
    else if (action == raw_rot_180) {
		settings.rawRot = settings.raw_rot[2];
		emit updateTextSignal("Camera Raw rotation - 180 (Horizontal)");
        qDebug() << "Camera Raw rotation set to 180 degree (Horizontal)";
	}
    else if (action == raw_rot_270) {
		settings.rawRot = settings.raw_rot[4];
		emit updateTextSignal("Camera Raw rotation - 90 CW (Vertical)");
		qDebug() << "Camera Raw rotation set to 90 degree CW (Vertical)";
	}
    
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