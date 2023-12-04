/*
 * Solidify (Push Pull) algorithm implementation using OpenImageIO
 * Copyright (c) 2023 Erium Vladlen.
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
    //alf_enable = new QAction("With Alpha", s_menu);
    //alf_enable->setCheckable(true);
    //alf_enable->setChecked(settings.alphaMode == 1);

    QAction* con_enable = new QAction("Enable Console", s_menu);
    con_enable->setCheckable(true);
    con_enable->setChecked(settings.conEnable);

    // Submenu
    QMenu* alf_submenu = new QMenu("Alpha/Mask", s_menu);
    QMenu* nm_submenu = new QMenu("Normalize", s_menu);
    QMenu* rep_submenu = new QMenu("Repair", s_menu);
    QMenu* rng_submenu = new QMenu("Floats type", s_menu);
    QMenu* fmt_submenu = new QMenu("Formats", s_menu);
    QMenu* bit_submenu = new QMenu("Bits Depth", s_menu);
    QMenu* raw_submenu = new QMenu("Camera Raw", s_menu);

    QActionGroup* NormGroup = new QActionGroup(nm_submenu);
    QActionGroup* AlfGroup = new QActionGroup(alf_submenu);
    QActionGroup* RepairGroup = new QActionGroup(rep_submenu);
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
    // Alpha/Mask
    alf_Disb = createAction("Disable", AlfGroup, alf_submenu, true, (settings.alphaMode == 0));
    alf_Embed = createAction("Preserve", AlfGroup, alf_submenu, true, (settings.alphaMode == 1));
    alf_Only = createAction("Export", AlfGroup, alf_submenu, true, (settings.alphaMode == 2));

    // Normals
    nrm_Dis = createAction("Disable", NormGroup, nm_submenu, true, (settings.normMode == 0));
    nrm_Smrt = createAction("Smart", NormGroup, nm_submenu, true, (settings.normMode == 1));
    nrm_Force = createAction("Force", NormGroup, nm_submenu, true, (settings.normMode == 2));

    // Repair
    rep_Dis = createAction("Disable", RepairGroup, rep_submenu, true, (settings.repairMode == 0));
    rep_Z = createAction("X", RepairGroup, rep_submenu, true, (settings.repairMode == 1));
    rep_Y = createAction("Y", RepairGroup, rep_submenu, true, (settings.repairMode == 2));
    rep_X = createAction("Z", RepairGroup, rep_submenu, true, (settings.repairMode == 3));
    rep_mZ = createAction("-X", RepairGroup, rep_submenu, true, (settings.repairMode == 4));
    rep_mY = createAction("-Y", RepairGroup, rep_submenu, true, (settings.repairMode == 5));
    rep_mX = createAction("-Z", RepairGroup, rep_submenu, true, (settings.repairMode == 6));

    // Range
    rng_Unsg = createAction("Unsigned", RangeGroup, rng_submenu, true, (settings.rangeMode == 0));
    rng_Sign = createAction("Signed", RangeGroup, rng_submenu, true, (settings.rangeMode == 1));
    rng_SU = createAction("Signed <> Unsigned", RangeGroup, rng_submenu, true, (settings.rangeMode == 2));
    rng_US = createAction("Unsigned <> Signed", RangeGroup, rng_submenu), true, (settings.rangeMode == 3);

    // File Formats
    frmt_Org = createAction("Original", FrmtGroup, fmt_submenu, true, (settings.fileFormat == -1));
    frmt_Tif = createAction("TIFF", FrmtGroup, fmt_submenu, true, (settings.fileFormat == 0));
    frmt_Exr = createAction("OpenEXR", FrmtGroup, fmt_submenu, true, (settings.fileFormat == 1));
    frmt_Png = createAction("PNG", FrmtGroup, fmt_submenu, true, (settings.fileFormat == 2));
    frmt_Jpg = createAction("JPEG", FrmtGroup, fmt_submenu, true, (settings.fileFormat == 3));
    frmt_Jp2 = createAction("JPEG-2000", FrmtGroup, fmt_submenu, true, (settings.fileFormat == 4));
    frmt_Ppm = createAction("PPM", FrmtGroup, fmt_submenu, true, (settings.fileFormat == 5));

    // Bits Depth
    bit_orig = createAction("Original", BitsGroup, bit_submenu, true, (settings.bitDepth == -1));
    bit_uint8 = createAction("8 bits int", BitsGroup, bit_submenu, true, (settings.bitDepth == 0));
    bit_uint16 = createAction("16 bits int", BitsGroup, bit_submenu, true, (settings.bitDepth == 1));
    bit_uint32 = createAction("32 bits int", BitsGroup, bit_submenu, true, (settings.bitDepth == 2));
    bit_uint64 = createAction("64 bits int", BitsGroup, bit_submenu, true, (settings.bitDepth == 3));
    bit_flt16 = createAction("16 bits float", BitsGroup, bit_submenu, true, (settings.bitDepth == 4));
    bit_flt32 = createAction("32 bits float", BitsGroup, bit_submenu, true, (settings.bitDepth == 5));
	bit_flt64 = createAction("64 bits float", BitsGroup, bit_submenu, true, (settings.bitDepth == 6));

    // camera raw rotation
    raw_rot_A = createAction("Auto EXIF", RawGroup, raw_submenu, true, (settings.rawRot == -1));
    raw_rot_0 = createAction("0 Horizontal", RawGroup, raw_submenu, true, (settings.rawRot == 0));
    raw_rot_90 = createAction("-90 Vertical", RawGroup, raw_submenu, true, (settings.rawRot == 3));
    raw_rot_270 = createAction("+90 Vertical", RawGroup, raw_submenu, true, (settings.rawRot == 5));
    raw_rot_180 = createAction("180 Horizontal", RawGroup, raw_submenu, true, (settings.rawRot == 6));

    s_menu->addAction(sld_enable);
    s_menu->addSeparator();
    s_menu->addMenu(alf_submenu);
    s_menu->addMenu(nm_submenu);
    s_menu->addMenu(rep_submenu);
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
    setWindowTitle(QString("Solidify ToolBox %1.%2").arg(VERSION_MAJOR).arg(VERSION_MINOR));
    setFixedSize(500, 500);

    // Connect the signal from the drop area to the slot in the main window
    connect(dropArea, &DropArea::filesDropped, this, &MainWindow::startProcessing);

    // Connect the Settings action's triggered signal

    connect(sld_enable, &QAction::toggled, this, &MainWindow::sldfSettings);
    //connect(alf_enable, &QAction::toggled, this, &MainWindow::alfSettings);
    QList<QAction*> alf_act = { alf_Disb, alf_Embed, alf_Only };
    for (QAction* action : alf_act) {
		connect(action, &QAction::triggered, this, &MainWindow::alfSettings);
	}
    QList<QAction*> norm_act = { nrm_Dis, nrm_Smrt, nrm_Force };
    for (QAction* action : norm_act) {
		connect(action, &QAction::triggered, this, &MainWindow::normSettings);
	}
    QList<QAction*> rep_act = { rep_Dis, rep_Z, rep_Y, rep_X, rep_mZ, rep_mY, rep_mX };
    for (QAction* action : rep_act) {
        connect(action, &QAction::triggered, this, &MainWindow::repSettings);
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

    connect(this, &MainWindow::changeProgressBarColorSignal, this, &MainWindow::changeProgressBarColorSlot);
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

void MainWindow::repSettings() {
    std::vector<std::pair<QString, uint>> actionMap = 
    { { "Disable", 0 }, { "X", 1 }, { "Y", 2 }, { "Z", 3 },
	{ "-X", 4 }, { "-Y", 5 }, { "-Z", 6 } };
    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < actionMap.size(); i++) {
        if (action->text() == actionMap[i].first) {
			settings.repairMode = actionMap[i].second;
			emit updateTextSignal(QString("Repair mode set to %1").arg(actionMap[i].first));
			qDebug() << QString("Repair mode set to %1").arg(actionMap[i].first);
			break;
		}
	}
    if (action != rep_Dis) {
        nrm_Dis->setChecked(true);
        settings.normMode = 0;
        emit updateTextSignal("Normalizing are part of Repair");
        qDebug() << "Normalizing are part of Repair";
    }
}

void MainWindow::sldfSettings(bool checked) {
    settings.isSolidify = sld_enable->isChecked();
    if (checked) {
        //alf_enable->setChecked(false);
        emit updateTextSignal("Solidify Enabled");
        qDebug() << "Solidify Enabled";
        emit updateTextSignal("Export Alpha Disabled");
        qDebug() << "Export Alpha Disabled";
    }
    else {
        //nrm_Smrt->setChecked(false);
        //nrm_Dis->setChecked(true);
        //settings.normMode = 0;
        emit updateTextSignal("Solidify Disabled");
        //emit updateTextSignal("Normalizing disabled");
        //qDebug() << "Normalizing disabled";
        qDebug() << "Solidify Disabled";
    }
}

void MainWindow::alfSettings() {
    std::vector<std::pair<QString, uint>> actionMap ={ { "Disable", 0 }, { "Preserve", 1 }, { "Export", 2 } };
    QAction* action = qobject_cast<QAction*>(sender());
    for (int i = 0; i < actionMap.size(); i++) {
        if (action->text() == actionMap[i].first) {
            settings.alphaMode = actionMap[i].second;
            emit updateTextSignal(QString("Alpha/Mask set to %1").arg(actionMap[i].first));
            qDebug() << QString("Alpha/Mask set to %1").arg(actionMap[i].first);
            break;
        }
    }
    if (action == alf_Only) {
        sld_enable->setChecked(false);
        emit updateTextSignal("Solidify Disabled");
        qDebug() << "Solidify Disabled";
        settings.isSolidify = false;

        nrm_Dis->setChecked(true);
        settings.normMode = 0;
        emit updateTextSignal("Normalizing disabled");
        qDebug() << "Normalizing disabled";

        rep_Dis->setChecked(true);
        settings.repairMode = 0;
        emit updateTextSignal("Repair mode set to Disable");
        qDebug() << "Repair mode set to Disable";
    }
}

void MainWindow::rngSettings() {
	QAction* action = qobject_cast<QAction*>(sender());
    if (action == rng_Unsg) {
        settings.rangeMode = 0;
		emit updateTextSignal("Unsigned floats");
        qDebug() << "32/16 bit floats are in [0.0 ~ 1.0] range";
	}
    else if (action == rng_Sign) {
        settings.rangeMode = 1;
		emit updateTextSignal("Signed floats");
        qDebug() << "32/16 bit floats are in [-1.0 ~ 1.0] range";
	}
    else if (action == rng_US) {
        settings.rangeMode = 2;
        emit updateTextSignal("Unsigned <> Signed");
        qDebug() << "32/16 bit floats range [0.0 ~ 1.0] map to a [-1.0 ~ 1.0]";
    }
    else if (action == rng_SU) {
		settings.rangeMode = 3;
        emit updateTextSignal("Signed <> Unsigned");
        qDebug() << "32/16 bit floats range [-1.0 ~ 1.0] map to a [0.0 ~ 1.0]";
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

    ShowWindow(GetConsoleWindow(), SW_HIDE);
    FreeConsole();

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