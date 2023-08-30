/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2017
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include"utils/pi_time.h"

#include"perfutil_core.hpp"
#include"PiFacility.hpp"
#include"RTSampleProcessor.hpp"
#include"TaskInfo.hpp"

namespace pi {
namespace pu {

RTSampleProcessor::RTSampleProcessor() {
}

RTSampleProcessor::~RTSampleProcessor() {
}

int RTSampleProcessor::initialize(std::string &name, PiFacility *facility) {
   piFacility = facility;
   ss << "STATUS OFF" << std::endl;
   return 0;
}

int RTSampleProcessor::startedRecording() {
   ss.str("");
   ss.clear();
   ss << "STATUS RUNNING" << std::endl;
   // TODO: write some information about the tracing
   // event type and period/frequency

   // about the OS. number of active CPUs
   return 0;
}

int RTSampleProcessor::stoppedRecording() {
   ss << "STATUS OFF" << std::endl;
   return 0;
}

int RTSampleProcessor::terminate() {
   ss.str("");
   ss.clear();
   return 0;
}

int RTSampleProcessor::taskName(RecordHeader &header, std::string &tname) {
   // "mte.taskName <pid> <tid> <name>\n"
   ss << "mte.taskName " << std::hex << header.pid << " " << std::hex
         << header.tid << " " << tname << std::endl;
   return 0;
}

int RTSampleProcessor::taskStart(RecordHeader &header, bool newProc) {
   return 0;
}

int RTSampleProcessor::taskEnd(RecordHeader &header) {
   return 0;
}

int RTSampleProcessor::memorySegment(RecordHeader &header, uint64_t address,
                                     uint64_t size, std::string &name) {
   // "mte.moduleName <pid> <moduleId> <name>\n"
   ss << "mte.moduleName " << std::hex << header.pid << " " << std::hex
         << header.itemId << " " << name << std::endl;
   return 0;
}

int RTSampleProcessor::symbol(RecordHeader &header, uint64_t moduleId,
                              uint64_t address, uint64_t size,
                              std::string &name) {
   // "mte.symbolName <pid> <moduleId> <symbolId> <name>\n"
   ss << "mte.symbolName " << std::hex << header.pid << " " << std::hex
         << moduleId << " " << std::hex << header.itemId << " " << name
         << std::endl;
   return 0;
}

int RTSampleProcessor::sample(SampleRecord &sample) {
   ResolvedSymbol symbol;

   // the task info will handle non resolvable samples by giving blank ids
   // and maintaining the original address
   piFacility->taskInfo->resolveSymbol(sample, symbol);

   // "sample <timeStamp> <pid> <tid> <moduleId> <symbolId> <offset>\n"
   ss << "sample " << std::hex << sample.timeStamp << " " << std::hex
         << sample.pid << " " << std::hex << sample.tid << " " << std::hex
         << symbol.moduleId << " " << std::hex << symbol.symbolId << " "
         << std::hex << symbol.offset << std::endl;
   return 0;
}

std::string RTSampleProcessor::getUnreadData() {
   std::string data = ss.str();
   ss.str("");
   ss.clear();
   return data;
}

}
}
