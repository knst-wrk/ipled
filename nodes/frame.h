/** This file is part of ipled - a versatile LED strip controller.
Copyright (C) 2024 Sven Pauli <sven@knst-wrk.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef FRAME_H
#define FRAME_H

#include "ui_frame.h"

class QModelIndex;

class FramesModel;

class Frame:
    public QDialog,
    private Ui::Frame
{
    Q_OBJECT

public:
    explicit Frame(QWidget *parent = 0);
    ~Frame();

protected:
    void changeEvent(QEvent *e);

private:
    FramesModel *framesModel;

private slots:
    void pickFile();
    void addFile(const QString &file);
    void loadFile(const QString &file);

    void sendFrame(const QModelIndex &index);
};

#endif // DIALOG_H
