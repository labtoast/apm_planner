/*=====================================================================

PIXHAWK Micro Air Vehicle Flying Robotics Toolkit

(c) 2009, 2010 PIXHAWK PROJECT  <http://pixhawk.ethz.ch>

This file is part of the PIXHAWK project

    PIXHAWK is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PIXHAWK is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PIXHAWK. If not, see <http://www.gnu.org/licenses/>.

======================================================================*/

/**
 * @file
 *   @brief List of unmanned vehicles
 *   @author Lorenz Meier <mavteam@student.ethz.ch>
 *
 */

#include <QString>
#include <QTimer>
#include <QLabel>
#include <QFileDialog>

#include <QApplication>

#include "MG.h"
#include "UASListWidget.h"
#include "UASManager.h"
#include "UAS.h"
#include "UASView.h"
#include "QGCUnconnectedInfoWidget.h"
#include "MainWindow.h"
#include "MAVLinkSimulationLink.h"
#include "LinkManager.h"

UASListWidget::UASListWidget(QWidget *parent) : QWidget(parent),
    uWidget(NULL),
    m_ui(new Ui::UASList)
{
    m_ui->setupUi(this);

    listLayout = new QVBoxLayout(this);
    listLayout->setMargin(0);
    listLayout->setSpacing(3);
    listLayout->setAlignment(Qt::AlignTop);
    this->setLayout(listLayout);
    setObjectName("UNMANNED_SYSTEMS_LIST");

    // Construct initial widget
    //uWidget = new QGCUnconnectedInfoWidget(this);
    //listLayout->addWidget(uWidget);

    this->setMinimumWidth(262);

    uasViews = QMap<UASInterface*, UASView*>();

    this->setVisible(false);

    connect(UASManager::instance(),SIGNAL(UASCreated(UASInterface*)),this,SLOT(addUAS(UASInterface*)));
    connect(UASManager::instance(),SIGNAL(UASDeleted(UASInterface*)),this,SLOT(removeUAS(UASInterface*)));

    // Get a list of all existing UAS
    foreach (UASInterface* uas, UASManager::instance()->getUASList()) {
        addUAS(uas);
    }
}

UASListWidget::~UASListWidget()
{
    delete m_ui;
}

void UASListWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type())
    {
    case QEvent::LanguageChange:
        m_ui->retranslateUi(this);
        break;
    default:
        break;
    }
}



void UASListWidget::addUAS(UASInterface* uas)
{
    if (uasViews.isEmpty())
    {
        if (uWidget)
        {
            listLayout->removeWidget(uWidget);
            delete uWidget;
            uWidget = NULL;
        }
    }

    if (!uasViews.contains(uas))
    {
        uasViews.insert(uas, new UASView(uas, this));

	// always display gcs at top of uas list
	if (uas->getUASID() == 255)
	{
	    listLayout->insertWidget(0, uasViews.value(uas));
	}
	else
	{
	    listLayout->addWidget(uasViews.value(uas));
	}
        //connect(uas, SIGNAL(destroyed(QObject*)), this, SLOT(removeUAS(QObject*)));
    }
}

void UASListWidget::activeUAS(UASInterface* uas)
{
    UASView* view = uasViews.value(uas, NULL);
    if (view) {
        view->setUASasActive(true);
    }
}

void UASListWidget::removeUAS(UASInterface* uas)
{
    if (uasViews.contains(uas))
    {
        listLayout->removeWidget(uasViews.value(uas));
        delete uasViews.value(uas);
        uasViews.remove(uas);


    }
}

