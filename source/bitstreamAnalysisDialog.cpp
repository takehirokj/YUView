/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*   
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "bitstreamAnalysisDialog.h"

#include "parserAnnexBAVC.h"
#include "parserAnnexBHEVC.h"
#include "parserAnnexBMpeg2.h"
#include "parserAVFormat.h"

bitstreamAnalysisDialog::bitstreamAnalysisDialog(QWidget *parent, QString fileName, inputFormat inputFormatType) :
  QDialog(parent)
{
  ui.setupUi(this);

  statusBar = new QStatusBar;
  ui.verticalLayout->addWidget(statusBar);

  if (inputFormatType == inputInvalid)
    return;

  // Setup the parser
  if (inputFormatType == inputAnnexBHEVC)
    parser.reset(new parserAnnexBHEVC(this));
  else if (inputFormatType == inputAnnexBAVC)
    parser.reset(new parserAnnexBAVC(this));
  else if (inputFormatType == inputLibavformat)
    parser.reset(new parserAVFormat(this));
  
  parser->enableModel();

  ui.streamInfoTreeWidget->setColumnWidth(0, 300);

  ui.dataTreeView->setModel(parser->getPacketItemModel());
  ui.dataTreeView->setColumnWidth(0, 600);
  ui.dataTreeView->setColumnWidth(1, 100);
  ui.dataTreeView->setColumnWidth(2, 120);

  compressedFilePath = fileName;

  connect(parser.data(), &parserBase::nalModelUpdated, this, &bitstreamAnalysisDialog::updateParserItemModel);
  connect(parser.data(), &parserBase::streamInfoUpdated, this, &bitstreamAnalysisDialog::updateStreamInfo);
  connect(parser.data(), &parserBase::backgroundParsingDone, this, &bitstreamAnalysisDialog::backgroundParsingDone);
  
  connect(ui.showVideoStreamOnlyCheckBox, &QCheckBox::toggled, this, &bitstreamAnalysisDialog::showVideoStreamOnlyCheckBoxToggled);
  connect(ui.colorCodeStreamsCheckBox, &QCheckBox::toggled, this, &bitstreamAnalysisDialog::colorCodeStreamsCheckBoxToggled);

  // Start the background parsing thread
  statusBar->showMessage("Parsing file ...");
  backgroundParserFuture = QtConcurrent::run(this, &bitstreamAnalysisDialog::backgroundParsingFunction);

  updateStreamInfo();
}

bitstreamAnalysisDialog::~bitstreamAnalysisDialog()
{
  // If the background thread is still working, abort it.
  if (backgroundParserFuture.isRunning())
  {
    // signal to background thread that we want to cancel the processing
    parser->setAbortParsing();
    backgroundParserFuture.waitForFinished();
  }
}

void bitstreamAnalysisDialog::updateParserItemModel(unsigned int newNumberItems)
{
  parser->setNewNumberModelItems(newNumberItems);
  statusBar->showMessage(QString("Parsing file (%1%)").arg(parser->getParsingProgressPercent()));
}

void bitstreamAnalysisDialog::updateStreamInfo()
{
  ui.streamInfoTreeWidget->clear();
  ui.streamInfoTreeWidget->addTopLevelItems(parser->getStreamInfo());
  ui.streamInfoTreeWidget->expandAll();
  ui.showVideoStreamOnlyCheckBox->setEnabled(parser->getNrStreams() > 1);
}

void bitstreamAnalysisDialog::backgroundParsingDone(QString error)
{
  if (error.isEmpty())
    statusBar->showMessage("Parsing done.");
  else
    statusBar->showMessage("Error parsing the file: " + error);
}

void bitstreamAnalysisDialog::showVideoStreamOnlyCheckBoxToggled(bool state)
{
  if (showVideoStreamOnly != state)
  {
    showVideoStreamOnly = state;
    if (showVideoStreamOnly)
      parser->setFilterStreamIndex(parser->getVideoStreamIndex());
    else
      parser->setFilterStreamIndex(-1);
  }
}

void bitstreamAnalysisDialog::backgroundParsingFunction()
{
  parser->runParsingOfFile(compressedFilePath);
}
