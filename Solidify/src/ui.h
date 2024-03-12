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
#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMenu>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtCore/QMimeData>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QRandomGenerator>

#include "processing.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 39

void setPBarColor(QProgressBar* progressBar, const QColor& color = QColor("#05B8CC"));

class DropArea : public QLabel {
    Q_OBJECT  // Macro needed to handle signals and slots

public:
    DropArea();

signals:
    void filesDropped(QList<QUrl> urls);  // New signal to be emitted when files are dropped

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

class MainWindow : public QMainWindow {
    Q_OBJECT  // Macro needed to handle signals and slots
public:
    MainWindow();
    void emitUpdateTextSignal(const QString& text) { emit updateTextSignal(text); }  // Public method to emit the signal

signals:
    void updateTextSignal(const QString& text);
    void changeProgressBarColorSignal(QColor color);

public slots:
    void setProgressBarValueSlot(int value) { progressBar->setValue(value); }
    void changeProgressBarColorSlot(const QColor& color) { setPBarColor(progressBar, color); }

private slots:
    void restartApp();
    void toggleConsole(bool cheked);
    void startProcessing(QList<QUrl> urls);
    void normSettings();
    void repSettings();
    void sldfSettings(bool checked);
    void rngSettings();
    void frmtSettings();
    void bitSettings();
    void alfSettings();
    void rawSettings();

private:
    QFutureWatcher<bool> processingWatcher;
    QProgressBar* progressBar;

    QAction* sld_enable;

    QAction* alf_Disb;
    QAction* alf_Embed;
    QAction* alf_Only;

    QAction* con_enable;

    QAction* nrm_Dis;
    QAction* nrm_Smrt;
    QAction* nrm_Force;

    QAction* rep_Dis;
    QAction* rep_Z;
    QAction* rep_Y;
    QAction* rep_X;
    QAction* rep_mZ;
    QAction* rep_mY;
    QAction* rep_mX;

    QAction* rng_Unsg;
    QAction* rng_Sign;
    QAction* rng_SU;
    QAction* rng_US;

    QAction* frmt_Org; // Original
    QAction* frmt_Png; // PNG
    QAction* frmt_Jpg; // JPG
    QAction* frmt_Tif; // TIFF
    QAction* frmt_Exr; // OpenEXR
    QAction* frmt_Jp2; // JPEG 2000
    QAction* frmt_Hic; // HEIC
    QAction* frmt_Ppm; // PPM

    //std::vector<QAction*> bitActions;
    QAction* bit_orig;
    QAction* bit_uint8;
    QAction* bit_uint16;
    QAction* bit_uint32;
    QAction* bit_uint64;
    QAction* bit_flt16;
    QAction* bit_flt32;
    QAction* bit_flt64;

    QAction* raw_rot_A;
    QAction* raw_rot_0;
    QAction* raw_rot_90;
    QAction* raw_rot_180;
    QAction* raw_rot_270;
};

