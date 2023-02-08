/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2009
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

#ifndef RT_SAMPLE_PROCESSOR_H
#define RT_SAMPLE_PROCESSOR_H

#include <sstream>

#include "SampleProcessor.hpp"

namespace pi {
namespace pu {

class RTSampleProcessor : public SampleProcessor {
public:
   RTSampleProcessor();
   virtual ~RTSampleProcessor();

   virtual int initialize(std::string &name, PiFacility *facility);
   virtual int startedRecording();
   virtual int stoppedRecording();
   virtual int terminate();

   virtual int taskName(RecordHeader &header, std::string &tname);
   virtual int taskStart(RecordHeader &header, bool newProc);
   virtual int taskEnd(RecordHeader &header);
   virtual int memorySegment(RecordHeader &header, uint64_t address, uint64_t size, std::string &name);
   virtual int symbol(RecordHeader &header, uint64_t moduleId, uint64_t address, uint64_t size, std::string &name);
   virtual int sample(SampleRecord &sample);

   std::string getUnreadData();

private:
   std::stringstream ss;
};

}
}

#endif				/* NRM2_SAMPLE_PROCESSOR_H */
