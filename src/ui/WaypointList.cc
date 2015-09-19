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
 *   @brief Waypoint list widget
 *
 *   @author Lorenz Meier <mavteam@student.ethz.ch>
 *   @author Benjamin Knecht <mavteam@student.ethz.ch>
 *   @author Petri Tanskanen <mavteam@student.ethz.ch>
 *
 */

#include "WaypointList.h"
#include "ui_WaypointList.h"
#include <UASInterface.h>
#include <UAS.h>
#include <UASManager.h>

#include <QFileDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include "LinkManager.h"

WaypointList::WaypointList(QWidget *parent, UASWaypointManager* wpm) :
    QWidget(parent),
    m_uas(NULL),
    WPM(wpm),
    mavX(0.0),
    mavY(0.0),
    mavZ(0.0),
    mavYaw(0.0),
    showOfflineWarning(false),
    m_ui(new Ui::WaypointList)
{

    m_ui->setupUi(this);

    //EDIT TAB

    editableListLayout = new QVBoxLayout(m_ui->editableListWidget);
    editableListLayout->setSpacing(0);
    editableListLayout->setMargin(0);
    editableListLayout->setAlignment(Qt::AlignTop);
    m_ui->editableListWidget->setLayout(editableListLayout);
    m_ui->wpRadiusSpinBox->setEnabled(false);

    // ADD WAYPOINT
    // Connect add action, set right button icon and connect action to this class
    connect(m_ui->addButton, SIGNAL(clicked()), m_ui->actionAddWaypoint, SIGNAL(triggered()));
    connect(m_ui->actionAddWaypoint, SIGNAL(triggered()), this, SLOT(addEditable()));

    // ADD WAYPOINT AT CURRENT POSITION
    connect(m_ui->positionAddButton, SIGNAL(clicked()), this, SLOT(addCurrentPositionWaypoint()));

    // SEND WAYPOINTS
    connect(m_ui->transmitButton, SIGNAL(clicked()), this, SLOT(transmit()));

    // DELETE ALL WAYPOINTS
    connect(m_ui->clearWPListButton, SIGNAL(clicked()), this, SLOT(clearWPWidget()));

//    // REQUEST WAYPOINTS
//    connect(m_ui->readButton, SIGNAL(clicked()), this, SLOT(read()));

    // SAVE/LOAD WAYPOINTS
    connect(m_ui->saveButton, SIGNAL(clicked()), this, SLOT(saveWaypoints()));
    connect(m_ui->loadButton, SIGNAL(clicked()), this, SLOT(loadWaypoints()));

    connect(UASManager::instance(), SIGNAL(activeUASSet(UASInterface*)), this, SLOT(setUAS(UASInterface*)));

//    //VIEW TAB
//
//    viewOnlyListLayout = new QVBoxLayout(m_ui->viewOnlyListWidget);
//    viewOnlyListLayout->setSpacing(0);
//    viewOnlyListLayout->setMargin(0);
//    viewOnlyListLayout->setAlignment(Qt::AlignTop);
//    m_ui->viewOnlyListWidget->setLayout(viewOnlyListLayout);
//
//    // REFRESH VIEW TAB
//
//    connect(m_ui->refreshButton, SIGNAL(clicked()), this, SLOT(refresh()));

    if (WPM) {
        // SET UAS AFTER ALL SIGNALS/SLOTS ARE CONNECTED
        if (!WPM->getUAS())
        {
            // Hide buttons, which don't make sense without valid UAS
            m_ui->positionAddButton->setEnabled(false);
            m_ui->transmitButton->setEnabled(true);
//            m_ui->readButton->setEnabled(false);
//            m_ui->refreshButton->setEnabled(false);
//            //FIXME: The whole "Onboard Waypoints"-tab should be hidden, instead of "refresh" button
//            UnconnectedUASInfoWidget* inf = new UnconnectedUASInfoWidget(this);
//            viewOnlyListLayout->insertWidget(0, inf); //insert a "NO UAV" info into the Onboard Tab
            showOfflineWarning = true;
        } else {
            setUAS(static_cast<UASInterface*>(WPM->getUAS()));
        }

        /* connect slots */
        connect(WPM, SIGNAL(updateStatusString(const QString &)),        this, SLOT(updateStatusLabel(const QString &)));
        connect(WPM, SIGNAL(waypointEditableListChanged(void)),                  this, SLOT(waypointEditableListChanged(void)));
        connect(WPM, SIGNAL(waypointEditableChanged(int,Waypoint*)), this, SLOT(updateWaypointEditable(int,Waypoint*)));
        connect(WPM, SIGNAL(waypointViewOnlyListChanged(void)),                  this, SLOT(waypointViewOnlyListChanged(void)));
        connect(WPM, SIGNAL(waypointViewOnlyChanged(int,Waypoint*)), this, SLOT(updateWaypointViewOnly(int,Waypoint*)));
        connect(WPM, SIGNAL(currentWaypointChanged(quint16)),            this, SLOT(currentWaypointViewOnlyChanged(quint16)));

        m_ui->altSpinBox->setValue(WPM->getDefaultRelAltitude());
        connect(m_ui->altSpinBox, SIGNAL(valueChanged(double)), WPM, SLOT(setDefaultRelAltitude(double)));

        //Even if there are no waypoints, since this is a new instance and there is an
        //existing WPM, then we need to assume things have changed, and act appropriatly.
        waypointEditableListChanged();
        waypointViewOnlyListChanged();
    } else {
        QLOG_DEBUG() << "WaypointList: No WPM";
    }

    // STATUS LABEL
    updateStatusLabel("");

    setVisible(false);
    loadFileGlobalWP = false;
    readGlobalWP = false;
    centerMapCoordinate.setX(0.0);
    centerMapCoordinate.setY(0.0);

    // hide unused options
    m_ui->wpRadiusLabel->hide();
    m_ui->wpRadiusSpinBox->hide();
    m_ui->altLabel->hide();
    m_ui->altSpinBox->hide();
    m_ui->positionAddButton->hide();
    m_ui->addButton->hide();
    m_ui->clearWPListButton->hide();
}

WaypointList::~WaypointList()
{
    delete m_ui;
}

void WaypointList::updatePosition(UASInterface* uas, double x, double y, double z, quint64 usec)
{
    Q_UNUSED(uas);
    Q_UNUSED(usec);
    mavX = x;
    mavY = y;
    mavZ = z;
}

void WaypointList::updateAttitude(UASInterface* uas, double roll, double pitch, double yaw, quint64 usec)
{
    Q_UNUSED(uas);
    Q_UNUSED(usec);
    Q_UNUSED(roll);
    Q_UNUSED(pitch);
    mavYaw = yaw;
}

void WaypointList::setUAS(UASInterface* uas)
{
    if (m_uas != NULL)
    {
        // Clear current list
        on_clearWPListButton_clicked();
        // Disconnect everything
        disconnect(WPM, SIGNAL(updateStatusString(const QString &)),
                   this, SLOT(updateStatusLabel(const QString &)));
        disconnect(WPM, SIGNAL(waypointEditableListChanged(void)),
                   this, SLOT(waypointEditableListChanged(void)));
        disconnect(WPM, SIGNAL(waypointEditableChanged(int,Waypoint*)),
                   this, SLOT(updateWaypointEditable(int,Waypoint*)));
        disconnect(WPM, SIGNAL(waypointViewOnlyListChanged(void)),
                   this, SLOT(waypointViewOnlyListChanged(void)));
        disconnect(WPM, SIGNAL(waypointViewOnlyChanged(int,Waypoint*)),
                   this, SLOT(updateWaypointViewOnly(int,Waypoint*)));
        disconnect(WPM, SIGNAL(currentWaypointChanged(quint16)),
                   this, SLOT(currentWaypointViewOnlyChanged(quint16)));
        disconnect(m_uas, SIGNAL(localPositionChanged(UASInterface*,double,double,double,quint64)),
                   this, SLOT(updatePosition(UASInterface*,double,double,double,quint64)));
        disconnect(m_uas, SIGNAL(attitudeChanged(UASInterface*,double,double,double,quint64)),
                   this, SLOT(updateAttitude(UASInterface*,double,double,double,quint64)));
        disconnect(m_uas,SIGNAL(parameterChanged(int,int,QString,QVariant)),
                   this,SLOT(parameterChanged(int,int,QString,QVariant)));
        disconnect(m_ui->wpRadiusSpinBox, SIGNAL(valueChanged(double)),
                   this, SLOT(wpRadiusChanged(double)));
        m_ui->wpRadiusSpinBox->setEnabled(false);
    }

    m_uas = uas;
    if (!uas)
        return;
    WPM = uas->getWaypointManager();

    connect(WPM, SIGNAL(updateStatusString(const QString &)),
            this, SLOT(updateStatusLabel(const QString &)));
    connect(WPM, SIGNAL(waypointEditableListChanged(void)),
            this, SLOT(waypointEditableListChanged(void)));
    connect(WPM, SIGNAL(waypointEditableChanged(int,Waypoint*)),
            this, SLOT(updateWaypointEditable(int,Waypoint*)));
    connect(WPM, SIGNAL(waypointViewOnlyListChanged(void)),
            this, SLOT(waypointViewOnlyListChanged(void)));
    connect(WPM, SIGNAL(waypointViewOnlyChanged(int,Waypoint*)),
            this, SLOT(updateWaypointViewOnly(int,Waypoint*)));
    connect(WPM, SIGNAL(currentWaypointChanged(quint16)),
            this, SLOT(currentWaypointViewOnlyChanged(quint16)));
    connect(uas, SIGNAL(localPositionChanged(UASInterface*,double,double,double,quint64)),
            this, SLOT(updatePosition(UASInterface*,double,double,double,quint64)));
    connect(uas, SIGNAL(attitudeChanged(UASInterface*,double,double,double,quint64)),
            this, SLOT(updateAttitude(UASInterface*,double,double,double,quint64)));
    connect(uas,SIGNAL(parameterChanged(int,int,QString,QVariant)),
               this,SLOT(parameterChanged(int,int,QString,QVariant)));;
    connect(m_ui->wpRadiusSpinBox, SIGNAL(valueChanged(double)),
               this, SLOT(wpRadiusChanged(double)));
    m_ui->wpRadiusSpinBox->setEnabled(true);

    // Update list
    read();
}

void WaypointList::saveWaypoints()
{
    QFileDialog *dialog = new QFileDialog(this,tr("Save File"), QGC::missionDirectory());
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setNameFilter("*.txt");
    dialog->selectFile("mission.txt");
    dialog->open(this, SLOT(saveDialogAccepted()));
    dialog->setFileMode(QFileDialog::AnyFile);
    connect(dialog,SIGNAL(accepted()),this,SLOT(saveWaypointsDialogAccepted()));
    dialog->show();
}
void WaypointList::saveWaypointsDialogAccepted()
{
    QFileDialog *dialog = qobject_cast<QFileDialog*>(sender());
    if (!dialog)
    {
        return;
    }
    if (dialog->selectedFiles().size() == 0)
    {
        //No file selected/cancel clicked
        return;
    }
    WPM->saveWaypoints(dialog->selectedFiles().at(0));
}

void WaypointList::loadWaypoints()
{
    QFileDialog *dialog = new QFileDialog(this, tr("Load File"), QGC::missionDirectory(), tr("Waypoint File (*.txt)"));
    dialog->setFileMode(QFileDialog::ExistingFile);
    connect(dialog,SIGNAL(accepted()),this,SLOT(loadWaypointsDialogAccepted()));
    dialog->show();
}
void WaypointList::loadWaypointsDialogAccepted()
{
    QFileDialog *dialog = qobject_cast<QFileDialog*>(sender());
    if (!dialog)
    {
        return;
    }
    if (dialog->selectedFiles().size() == 0)
    {
        //No file selected/cancel clicked
        return;
    }
    WPM->loadWaypoints(dialog->selectedFiles().at(0));
}

void WaypointList::transmit()
{
    if (m_uas)
    {
        WPM->writeWaypoints();
    }
}

void WaypointList::read()
{
    if (m_uas)
    {
        WPM->readWaypoints(true);
    }
}

void WaypointList::refresh()
{
    if (m_uas)
    {
        WPM->readWaypoints(false);
    }
}

void WaypointList::addEditable()
{
    addEditable(false);
}

void WaypointList::addEditable(bool onCurrentPosition)
{

    const QList<Waypoint *> &waypoints = WPM->getWaypointEditableList();
    Waypoint *wp;
    if (waypoints.count() > 0 &&
            !(onCurrentPosition && m_uas && (m_uas->localPositionKnown() || m_uas->globalPositionKnown())))
    {
        // Create waypoint with last frame
        Waypoint *last = waypoints.last();
        wp = WPM->createWaypoint();
//        wp->blockSignals(true);
        MAV_FRAME frame = static_cast<MAV_FRAME>(WPM->getFrameRecommendation());
        wp->setFrame(frame);
        if (frame == MAV_FRAME_GLOBAL || frame == MAV_FRAME_GLOBAL_RELATIVE_ALT)
        {
            wp->setLatitude(last->getLatitude());
            wp->setLongitude(last->getLongitude());
        } else {
            wp->setX(last->getX());
            wp->setY(last->getY());
            wp->setZ(last->getZ());
        }
        wp->setParam1(last->getParam1());
        wp->setParam2(last->getParam2());
        wp->setParam3(last->getParam3());
        wp->setParam4(last->getParam4());
        wp->setAutocontinue(last->getAutoContinue());
//        wp->blockSignals(false);
        wp->setAction(last->getAction());
//        WPM->addWaypointEditable(wp);
    }
    else
    {
        // [TODO] for APM should trigger a read if no WP0 exists.
        if (m_uas)
        {
            // Create first waypoint at current MAV position
            if (m_uas->globalPositionKnown())
            {
                if (onCurrentPosition)
                {
                    double alt;
                    if (WPM->getFrameRecommendation() == MAV_FRAME_GLOBAL) {
                        updateStatusLabel(tr("Added default GLOBAL (Abs alt.) waypoint."));
                        alt = m_uas->getAltitudeAMSL();
                    } else {
                        updateStatusLabel(tr("Added default GLOBAL (Relative alt.) waypoint."));
                        alt = m_uas->getAltitudeRelative();
                    }
                    wp = new Waypoint(0, m_uas->getLatitude(), m_uas->getLongitude(), alt, 0, WPM->getAcceptanceRadiusRecommendation(), 0, 0,true, true, (MAV_FRAME)WPM->getFrameRecommendation(), MAV_CMD_NAV_WAYPOINT);
                    WPM->addWaypointEditable(wp);

                } else {

                    if (WPM->getFrameRecommendation() == MAV_FRAME_GLOBAL) {
                        updateStatusLabel(tr("Added default GLOBAL (Abs alt.) waypoint."));
                    } else {
                        updateStatusLabel(tr("Added default GLOBAL (Relative alt.) waypoint."));
                    }
                    wp = new Waypoint(0, UASManager::instance()->getHomeLatitude(),
                                      UASManager::instance()->getHomeLongitude(),
                                      WPM->getAltitudeRecommendation((MAV_FRAME)WPM->getFrameRecommendation()),
                                      0,
                                      WPM->getAcceptanceRadiusRecommendation(), 0, 0,true, true,
                                      (MAV_FRAME)WPM->getFrameRecommendation(), MAV_CMD_NAV_WAYPOINT);
                    WPM->addWaypointEditable(wp);
                }
            }
            else if (m_uas->localPositionKnown())
            {
                if (onCurrentPosition)
                {
                    updateStatusLabel(tr("Added default LOCAL (NED) waypoint."));
                    wp = new Waypoint(0, m_uas->getLocalX(), m_uas->getLocalY(), m_uas->getLocalZ(), 0, WPM->getAcceptanceRadiusRecommendation(), 0, 0,true, true, MAV_FRAME_LOCAL_NED, MAV_CMD_NAV_WAYPOINT);
                    WPM->addWaypointEditable(wp);
                } else {
                    updateStatusLabel(tr("Added default LOCAL (NED) waypoint."));
                    wp = new Waypoint(0, 0, 0, -0.50, 0, WPM->getAcceptanceRadiusRecommendation(), 0, 0,true, true, MAV_FRAME_LOCAL_NED, MAV_CMD_NAV_WAYPOINT);
                    WPM->addWaypointEditable(wp);
                }
            }
            else
            {
                // MAV connected, but position unknown, add default waypoint
                updateStatusLabel(tr("WARNING: No position known. Adding default LOCAL (NED) waypoint"));
                wp = new Waypoint(0, UASManager::instance()->getHomeLatitude(),
                                  UASManager::instance()->getHomeLongitude(),
                                  WPM->getAltitudeRecommendation((MAV_FRAME)WPM->getFrameRecommendation()),
                                  0, WPM->getAcceptanceRadiusRecommendation(), 0, 0,true, true,
                                  (MAV_FRAME)WPM->getFrameRecommendation(), MAV_CMD_NAV_WAYPOINT);
                WPM->addWaypointEditable(wp);
            }
        }
        else
        {
            //Since no UAV available, create first default waypoint.
            updateStatusLabel(tr("No UAV connected. Adding default dummy HOME waypoint"));
            wp = new Waypoint(0, UASManager::instance()->getHomeLatitude(),
                              UASManager::instance()->getHomeLongitude(),
                              WPM->getAltitudeRecommendation((MAV_FRAME)WPM->getFrameRecommendation()),
                              0, WPM->getAcceptanceRadiusRecommendation(), 0, 0,true, true,
                              (MAV_FRAME)WPM->getFrameRecommendation(), MAV_CMD_NAV_WAYPOINT);
            WPM->addWaypointEditable(wp);
        }
    }
}

void WaypointList::addCurrentPositionWaypoint() {
    addEditable(true);
}

void WaypointList::updateStatusLabel(const QString &string)
{
    // Status label in write widget
    m_ui->statusLabel->setText(string);
    // Status label in read only widget
//    m_ui->viewStatusLabel->setText(string);
}

// Request UASWaypointManager to send the SET_CURRENT message to UAV
void WaypointList::changeCurrentWaypoint(quint16 seq)
{
    if (m_uas)
    {
        WPM->setCurrentWaypoint(seq);
    }
}

// Request UASWaypointManager to set the new "current" and make sure all other waypoints are not "current"
void WaypointList::currentWaypointEditableChanged(quint16 seq)
{
        WPM->setCurrentEditable(seq);
        const QList<Waypoint *> &waypoints = WPM->getWaypointEditableList();

        if (seq < waypoints.count())
        {
            for(int i = 0; i < waypoints.count(); i++)
            {
                WaypointEditableView* widget = wpEditableViews.find(waypoints[i]).value();
                if (!widget) continue;

                if (waypoints[i]->getId() == seq)
                {
                    widget->setCurrent(true);
                }
                else
                {
                    widget->setCurrent(false);
                }
            }
        }
}

// Update waypointViews to correctly indicate the new current waypoint
void WaypointList::currentWaypointViewOnlyChanged(quint16 seq)
{
    // First update the edit list
    currentWaypointEditableChanged(seq);

    const QList<Waypoint *> &waypoints = WPM->getWaypointViewOnlyList();

    if (seq < waypoints.count())
    {
        for(int i = 0; i < waypoints.count(); i++)
        {
            WaypointViewOnlyView* widget = wpViewOnlyViews.find(waypoints[i]).value();
            if (!widget) continue;

            if (waypoints[i]->getId() == seq)
            {
                widget->setCurrent(true);
            }
            else
            {
                widget->setCurrent(false);
            }
        }
    }
}

void WaypointList::updateWaypointEditable(int uas, Waypoint* wp)
{
    Q_UNUSED(uas);
    WaypointEditableView *wpv = wpEditableViews.value(wp);
    wpv->updateValues();
    m_ui->tabWidget->setCurrentIndex(0); // XXX magic number
}

void WaypointList::updateWaypointViewOnly(int uas, Waypoint* wp)
{
    Q_UNUSED(uas);
    WaypointViewOnlyView *wpv = wpViewOnlyViews.value(wp);
    wpv->updateValues();
    m_ui->tabWidget->setCurrentIndex(1); // XXX magic number
}

void WaypointList::waypointViewOnlyListChanged()
{
    // Prevent updates to prevent visual flicker
    setUpdatesEnabled(false);
    const QList<Waypoint *> &waypoints = WPM->getWaypointViewOnlyList();

    if (!wpViewOnlyViews.empty()) {
        QMapIterator<Waypoint*,WaypointViewOnlyView*> viewIt(wpViewOnlyViews);
        viewIt.toFront();
        while(viewIt.hasNext()) {
            viewIt.next();
            Waypoint *cur = viewIt.key();
            int i;
            for (i = 0; i < waypoints.count(); i++) {
                if (waypoints[i] == cur) {
                    break;
                }
            }
            if (i == waypoints.count()) {
                WaypointViewOnlyView* widget = wpViewOnlyViews.find(cur).value();
                widget->hide();
                viewOnlyListLayout->removeWidget(widget);
                wpViewOnlyViews.remove(cur);
            }
        }
    }

    // then add/update the views for each waypoint in the list
    for(int i = 0; i < waypoints.count(); i++) {
        Waypoint *wp = waypoints[i];
        if (!wpViewOnlyViews.contains(wp)) {
            WaypointViewOnlyView* wpview = new WaypointViewOnlyView(wp, this);
            wpViewOnlyViews.insert(wp, wpview);
            connect(wpview, SIGNAL(changeCurrentWaypoint(quint16)), this, SLOT(changeCurrentWaypoint(quint16)));
            viewOnlyListLayout->insertWidget(i, wpview);
        }
        WaypointViewOnlyView *wpv = wpViewOnlyViews.value(wp);

        //check if ordering has changed
        if(viewOnlyListLayout->itemAt(i)->widget() != wpv) {
            viewOnlyListLayout->removeWidget(wpv);
            viewOnlyListLayout->insertWidget(i, wpv);
        }

        wpv->updateValues();    // update the values of the ui elements in the view
    }
    setUpdatesEnabled(true);
    loadFileGlobalWP = false;

    m_ui->tabWidget->setCurrentIndex(1);

}

void WaypointList::waypointEditableListChanged()
{
    // Prevent updates to prevent visual flicker
    setUpdatesEnabled(false);
    const QList<Waypoint *> &waypoints = WPM->getWaypointEditableList();

    if (!wpEditableViews.empty()) {
        QMapIterator<Waypoint*,WaypointEditableView*> viewIt(wpEditableViews);
        viewIt.toFront();
        while(viewIt.hasNext()) {
            viewIt.next();
            Waypoint *cur = viewIt.key();
            int i;
            for (i = 0; i < waypoints.count(); i++) {
                if (waypoints[i] == cur) {
                    break;
                }
            }
            if (i == waypoints.count()) {
                WaypointEditableView* widget = wpEditableViews.find(cur).value();
                widget->hide();
                editableListLayout->removeWidget(widget);
                wpEditableViews.remove(cur);
            }
        }
    }

    // then add/update the views for each waypoint in the list
    for(int i = 0; i < waypoints.count(); i++) {
        Waypoint *wp = waypoints[i];
        if (!wpEditableViews.contains(wp)) {
            WaypointEditableView* wpview = new WaypointEditableView(wp, this);
            wpEditableViews.insert(wp, wpview);
            connect(wpview, SIGNAL(moveDownWaypoint(Waypoint*)),    this, SLOT(moveDown(Waypoint*)));
            connect(wpview, SIGNAL(moveUpWaypoint(Waypoint*)),      this, SLOT(moveUp(Waypoint*)));
            connect(wpview, SIGNAL(moveTopWaypoint(Waypoint*)),    this, SLOT(moveTop(Waypoint*)));
            connect(wpview, SIGNAL(moveBottomWaypoint(Waypoint*)),  this, SLOT(moveBottom(Waypoint*)));
            connect(wpview, SIGNAL(removeWaypoint(Waypoint*)),      this, SLOT(removeWaypoint(Waypoint*)));
            //connect(wpview, SIGNAL(currentWaypointChanged(quint16)), this, SLOT(currentWaypointChanged(quint16)));
            connect(wpview, SIGNAL(changeCurrentWaypoint(quint16)), this, SLOT(currentWaypointEditableChanged(quint16)));

            // don't show home waypoint
            if (wp->getId() == 0) {
              wpview->hide();
            }
            editableListLayout->insertWidget(i, wpview);
        }
        WaypointEditableView *wpv = wpEditableViews.value(wp);

        //check if ordering has changed
        if(editableListLayout->itemAt(i)->widget() != wpv) {
            editableListLayout->removeWidget(wpv);
            editableListLayout->insertWidget(i, wpv);
        }

        wpv->updateValues();    // update the values of the ui elements in the view
    }
    setUpdatesEnabled(true);
    loadFileGlobalWP = false;

}

void WaypointList::moveUp(Waypoint* wp)
{
    const QList<Waypoint *> &waypoints = WPM->getWaypointEditableList();

    //get the current position of wp in the local storage
    int i;
    for (i = 0; i < waypoints.count(); i++) {
        if (waypoints[i] == wp)
            break;
    }

    // if wp was found and its not the first entry, move it
    // For APM the first entry is WP1
    if (i < waypoints.count() && i > 1) {
        WPM->moveWaypoint(i, i-1);
    }
}

void WaypointList::moveDown(Waypoint* wp)
{    
    const QList<Waypoint *> &waypoints = WPM->getWaypointEditableList();

    //get the current position of wp in the local storage
    int i;
    // For APM entries start at WP1
    for (i = 1; i < waypoints.count(); i++) {
        if (waypoints[i] == wp)
            break;
    }

    // if wp was found and its not the last entry, move it
    if (i < waypoints.count()-1) {
        WPM->moveWaypoint(i, i+1);
    }
}

void WaypointList::moveTop(Waypoint* wp)
{
    const QList<Waypoint *> &waypoints = WPM->getWaypointEditableList();

    //get the current position of wp in the local storage
    int i;
    for (i = 0; i < waypoints.count(); i++) {
        if (waypoints[i] == wp)
            break;
    }

    // if wp was found and its not the first entry, move it
    // For APM the first entry is WP1
    if (i < waypoints.count() && i > 1) {
        WPM->moveWaypoint(i, 1);
    }
}

void WaypointList::moveBottom(Waypoint* wp)
{
    const QList<Waypoint *> &waypoints = WPM->getWaypointEditableList();

    //get the current position of wp in the local storage
    int i;
    // For APM entries start at WP1
    for (i = 1; i < waypoints.count(); i++) {
        if (waypoints[i] == wp)
            break;
    }

    // if wp was found and its not the last entry, move it
    if (i < waypoints.count()-1) {
        WPM->moveWaypoint(i, waypoints.count()-1);
    }
}

void WaypointList::removeWaypoint(Waypoint* wp)
{
    if (wp && (wp->getId() > 0)){ // APM use WP0 as home so do not remove it
        WPM->removeWaypoint(wp->getId());
    }
}

void WaypointList::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::LanguageChange:
        m_ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void WaypointList::on_clearWPListButton_clicked()
{
    if (m_uas) {
        emit clearPathclicked();
        const QList<Waypoint *> &waypoints = WPM->getWaypointEditableList();

        //Remove all but 1 waypoint, since the first is "home" on APM
        //Also, remove from the END first, work your way back to the first
        while (waypoints.size() > 1) {
            WaypointEditableView *widget = wpEditableViews.find(waypoints[waypoints.size()-1]).value();
            widget->remove();
        }
    }
}

void WaypointList::clearWPWidget()
{    
    // Get list
    const QList<Waypoint *> &waypoints = WPM->getWaypointEditableList();

    // XXX delete wps as well

    // Clear UI elements
    //Remove all but 1 waypoint, since the first is "home" on APM
    //Also, remove from the END first, work your way back to the first
    while(waypoints.size() > 1) {
        WaypointEditableView* widget = wpEditableViews.find(waypoints[waypoints.size()-1]).value();
        widget->remove();
    }
}

void WaypointList::wpRadiusChanged(double radius)
{
    if (m_uas){
        m_uas->setParameter(1,"WPNAV_RADIUS", radius*100.0); // WPNAV_RADIUS is in cm
    }
}

void WaypointList::parameterChanged(int uas, int component, QString parameterName, QVariant value)
{
    Q_UNUSED(uas);
    Q_UNUSED(component);

    if(parameterName.contains("WPNAV_RADIUS")){
        m_ui->wpRadiusSpinBox->setValue(value.toDouble()/100.0); // WPNAV_RADIUS is in cm
    }
}
