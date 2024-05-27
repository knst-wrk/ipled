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

#ifndef MATRIX_H
#define MATRIX_H

#include <Qt>

#include "ui_matrix.h"

class QModelIndex;
class QTimer;
class Dialog;

class Matrix:
    public QDialog,
    private Ui::Matrix
{
    Q_OBJECT

public:
    enum {
        SceneRole = Qt::UserRole + 1,
    } Roles;

    explicit Matrix(Dialog *parent);
    ~Matrix();

public slots:
    void activate(bool active);

protected:
    void changeEvent(QEvent *e);

private:
    class MatrixModel;
    class WeightItemDelegate;

    MatrixModel *m_model;

    QTimer *activator;

private slots:
    void addScene();
    void removeScene();
    void activatorTimeout();
    void setSpeed(int s);
};

#endif // DIALOG_H
