/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "ExecutionViewStep.h"

#include "Slideshow.h"

#include "Branding.h"
#include "CalamaresConfig.h"
#include "Job.h"
#include "JobQueue.h"
#include "Settings.h"
#include "ViewManager.h"
#include "modulesystem/Module.h"
#include "modulesystem/ModuleManager.h"
#include "utils/CalamaresUtilsGui.h"
#include "utils/Dirs.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"

#include <QDir>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

static Calamares::Slideshow*
makeSlideshow( QWidget* parent )
{
    const int api = Calamares::Branding::instance()->slideshowAPI();
    switch ( api )
    {
    case -1:
        return new Calamares::SlideshowPictures( parent );
#ifdef WITH_QML
    case 1:
        [[fallthrough]];
    case 2:
        return new Calamares::SlideshowQML( parent );
#endif
    default:
        cWarning() << "Unknown Branding slideshow API" << api;
        return new Calamares::SlideshowPictures( parent );
    }
}

namespace Calamares
{

ExecutionViewStep::ExecutionViewStep( QObject* parent )
    : ViewStep( parent )
    , m_widget( new QWidget )
    , m_progressBar( new QProgressBar )
    , m_label( new QLabel )
    , m_slideshow( makeSlideshow( m_widget ) )
{
    m_widget->setObjectName( "slideshow" );
    m_progressBar->setObjectName( "exec-progress" );
    m_label->setObjectName( "exec-message" );

    QVBoxLayout* layout = new QVBoxLayout( m_widget );
    QVBoxLayout* innerLayout = new QVBoxLayout;

    m_progressBar->setMaximum( 10000 );

    layout->addWidget( m_slideshow->widget() );
    CalamaresUtils::unmarginLayout( layout );
    layout->addLayout( innerLayout );

    innerLayout->addSpacing( CalamaresUtils::defaultFontHeight() / 2 );
    innerLayout->addWidget( m_progressBar );
    innerLayout->addWidget( m_label );

    connect( JobQueue::instance(), &JobQueue::progress, this, &ExecutionViewStep::updateFromJobQueue );
}


QString
ExecutionViewStep::prettyName() const
{
    return Calamares::Settings::instance()->isSetupMode() ? tr( "Set up" ) : tr( "Install" );
}


QWidget*
ExecutionViewStep::widget()
{
    return m_widget;
}


void
ExecutionViewStep::next()
{
}


void
ExecutionViewStep::back()
{
}


bool
ExecutionViewStep::isNextEnabled() const
{
    return false;
}


bool
ExecutionViewStep::isBackEnabled() const
{
    return false;
}


bool
ExecutionViewStep::isAtBeginning() const
{
    return true;
}


bool
ExecutionViewStep::isAtEnd() const
{
    return !JobQueue::instance()->isRunning();
}

void
ExecutionViewStep::onActivate()
{
    m_slideshow->changeSlideShowState( Slideshow::Start );

    const auto instanceDescriptors = Calamares::Settings::instance()->moduleInstances();

    JobQueue* queue = JobQueue::instance();
    for ( const auto& instanceKey : m_jobInstanceKeys )
    {
        const auto& moduleDescriptor = Calamares::ModuleManager::instance()->moduleDescriptor( instanceKey );
        Calamares::Module* module = Calamares::ModuleManager::instance()->moduleInstance( instanceKey );

        const auto instanceDescriptor
            = std::find_if( instanceDescriptors.constBegin(),
                            instanceDescriptors.constEnd(),
                            [=]( const Calamares::InstanceDescription& d ) { return d.key() == instanceKey; } );
        int weight = moduleDescriptor.weight();
        if ( instanceDescriptor != instanceDescriptors.constEnd() && instanceDescriptor->explicitWeight() )
        {
            weight = instanceDescriptor->weight();
        }
        weight = qBound( 1, weight, 100 );
        if ( module )
        {
            auto jl = module->jobs();
            if ( module->isEmergency() )
            {
                for ( auto& j : jl )
                {
                    j->setEmergency( true );
                }
            }
            queue->enqueue( weight, jl );
        }
    }

    queue->start();
}


JobList
ExecutionViewStep::jobs() const
{
    return JobList();
}


void
ExecutionViewStep::appendJobModuleInstanceKey( const ModuleSystem::InstanceKey& instanceKey )
{
    m_jobInstanceKeys.append( instanceKey );
}


void
ExecutionViewStep::updateFromJobQueue( qreal percent, const QString& message )
{
    m_progressBar->setValue( int( percent * m_progressBar->maximum() ) );
    if ( !message.isEmpty() )
    {
        m_label->setText( message );
    }
}

void
ExecutionViewStep::onLeave()
{
    m_slideshow->changeSlideShowState( Slideshow::Stop );
}

}  // namespace Calamares