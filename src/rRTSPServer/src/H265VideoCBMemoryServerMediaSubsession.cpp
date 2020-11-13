/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2020 Live Networks, Inc.  All rights reserved.
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from a H265 video circular buffer.
// Implementation

#include "H265VideoCBMemoryServerMediaSubsession.hh"
#include "H265VideoRTPSink.hh"
#include "ByteStreamCBMemorySource.hh"
#include "H265VideoStreamFramer.hh"

H265VideoCBMemoryServerMediaSubsession*
H265VideoCBMemoryServerMediaSubsession::createNew(UsageEnvironment& env,
                                                cb_output_buffer *cbBuffer,
                                                Boolean reuseFirstSource) {
    return new H265VideoCBMemoryServerMediaSubsession(env, cbBuffer, reuseFirstSource);
}

H265VideoCBMemoryServerMediaSubsession::H265VideoCBMemoryServerMediaSubsession(UsageEnvironment& env,
                                                                        cb_output_buffer *cbBuffer,
                                                                        Boolean reuseFirstSource)
    : CBMemoryServerMediaSubsession(env, cbBuffer, reuseFirstSource),
      fAuxSDPLine(NULL), fDoneFlag(0), fDummyRTPSink(NULL) {
}

H265VideoCBMemoryServerMediaSubsession::~H265VideoCBMemoryServerMediaSubsession() {
    delete[] fAuxSDPLine;
}

static void afterPlayingDummy(void* clientData) {
    H265VideoCBMemoryServerMediaSubsession* subsess = (H265VideoCBMemoryServerMediaSubsession*)clientData;
    subsess->afterPlayingDummy1();
}

void H265VideoCBMemoryServerMediaSubsession::afterPlayingDummy1() {
    // Unschedule any pending 'checking' task:
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    // Signal the event loop that we're done:
    setDoneFlag();
}

static void checkForAuxSDPLine(void* clientData) {
    H265VideoCBMemoryServerMediaSubsession* subsess = (H265VideoCBMemoryServerMediaSubsession*)clientData;
    subsess->checkForAuxSDPLine1();
}

void H265VideoCBMemoryServerMediaSubsession::checkForAuxSDPLine1() {
    nextTask() = NULL;

    char const* dasl;
    if (fAuxSDPLine != NULL) {
        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (fDummyRTPSink != NULL && (dasl = fDummyRTPSink->auxSDPLine()) != NULL) {
        fAuxSDPLine = strDup(dasl);
        fDummyRTPSink = NULL;

        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (!fDoneFlag) {
        // try again after a brief delay:
        int uSecsToDelay = 100000; // 100 ms
        nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
                              (TaskFunc*)checkForAuxSDPLine, this);
    }
}

char const* H265VideoCBMemoryServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
    if (fAuxSDPLine != NULL) return fAuxSDPLine; // it's already been set up (for a previous client)

    if (fDummyRTPSink == NULL) { // we're not already setting it up for another, concurrent stream
        // Note: For H265 video files, the 'config' information (used for several payload-format
        // specific parameters in the SDP description) isn't known until we start reading the file.
        // This means that "rtpSink"s "auxSDPLine()" will be NULL initially,
        // and we need to start reading data from our file until this changes.
        fDummyRTPSink = rtpSink;

        // Start reading the file:
        fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

        // Check whether the sink's 'auxSDPLine()' is ready:
        checkForAuxSDPLine(this);
    }

    envir().taskScheduler().doEventLoop(&fDoneFlag);

    return fAuxSDPLine;
}

FramedSource* H265VideoCBMemoryServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
    estBitrate = 500; // kbps, estimate

    // Create the video source:
    ByteStreamCBMemorySource* memorySource = ByteStreamCBMemorySource::createNew(envir(), fBuffer);
    if (memorySource == NULL) return NULL;

    // Create a framer for the Video Elementary Stream:
    return H265VideoStreamFramer::createNew(envir(), memorySource);
}

RTPSink* H265VideoCBMemoryServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
                   unsigned char rtpPayloadTypeIfDynamic,
                   FramedSource* /*inputSource*/) {
    return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
