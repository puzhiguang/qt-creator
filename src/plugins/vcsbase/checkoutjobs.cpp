/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "checkoutjobs.h"

#include <vcsbaseplugin.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <utils/synchronousprocess.h>

enum { debug = 0 };
namespace VCSBase {

AbstractCheckoutJob::AbstractCheckoutJob(QObject *parent) :
    QObject(parent)
{
}

struct ProcessCheckoutJobPrivate {
    ProcessCheckoutJobPrivate(const QString &binary,
                              const QStringList &args,
                              const QString &workingDirectory,
                              QProcessEnvironment env);

    QSharedPointer<QProcess> process;
    const QString binary;
    const QStringList args;
};

// Use a terminal-less process to suppress SSH prompts.
static inline QSharedPointer<QProcess> createProcess()
{
    unsigned flags = 0;
    if (VCSBasePlugin::isSshPromptConfigured())
        flags = Utils::SynchronousProcess::UnixTerminalDisabled;
    return Utils::SynchronousProcess::createProcess(flags);
}

ProcessCheckoutJobPrivate::ProcessCheckoutJobPrivate(const QString &b,
                              const QStringList &a,
                              const QString &workingDirectory,
                              QProcessEnvironment processEnv) :
    process(createProcess()),
    binary(b),
    args(a)
{    
    if (!workingDirectory.isEmpty())
        process->setWorkingDirectory(workingDirectory);
    VCSBasePlugin::setProcessEnvironment(&processEnv, false);
    process->setProcessEnvironment(processEnv);
}

ProcessCheckoutJob::ProcessCheckoutJob(const QString &binary,
                                       const QStringList &args,
                                       const QString &workingDirectory,
                                       const QProcessEnvironment &env,
                                       QObject *parent) :
    AbstractCheckoutJob(parent),
    d(new ProcessCheckoutJobPrivate(binary, args, workingDirectory, env))
{
    if (debug)
        qDebug() << "ProcessCheckoutJob" << binary << args << workingDirectory;
    connect(d->process.data(), SIGNAL(error(QProcess::ProcessError)), this, SLOT(slotError(QProcess::ProcessError)));
    connect(d->process.data(), SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(slotFinished(int,QProcess::ExitStatus)));
    connect(d->process.data(), SIGNAL(readyReadStandardOutput()), this, SLOT(slotOutput()));
    d->process->setProcessChannelMode(QProcess::MergedChannels);
    d->process->closeWriteChannel();
}

ProcessCheckoutJob::~ProcessCheckoutJob()
{
    delete d;
}

void ProcessCheckoutJob::slotOutput()
{
    const QByteArray data = d->process->readAllStandardOutput();
    const QString s = QString::fromLocal8Bit(data, data.endsWith('\n') ? data.size() - 1: data.size());
    if (debug)
        qDebug() << s;
    emit output(s);
}

void ProcessCheckoutJob::slotError(QProcess::ProcessError error)
{
    switch (error) {
    case QProcess::FailedToStart:
        emit failed(tr("Unable to start %1: %2").
                    arg(QDir::toNativeSeparators(d->binary), d->process->errorString()));
        break;
    default:
        emit failed(d->process->errorString());
        break;
    }
}

void ProcessCheckoutJob::slotFinished (int exitCode, QProcess::ExitStatus exitStatus)
{
    if (debug)
        qDebug() << "finished" << exitCode << exitStatus;

    switch (exitStatus) {
    case QProcess::NormalExit:
        emit output(tr("The process terminated with exit code %1.").arg(exitCode));
        if (exitCode == 0) {
            emit succeeded();
        } else {
            emit failed(tr("The process returned exit code %1.").arg(exitCode));
        }
        break;
    case QProcess::CrashExit:
        emit failed(tr("The process terminated in an abnormal way."));
        break;
    }
}

void ProcessCheckoutJob::start()
{
    d->process->start(d->binary, d->args);
}

void ProcessCheckoutJob::cancel()
{
    if (debug)
        qDebug() << "ProcessCheckoutJob::start";

    emit output(tr("Stopping..."));
    Utils::SynchronousProcess::stopProcess(*d->process);
}

} // namespace VCSBase
