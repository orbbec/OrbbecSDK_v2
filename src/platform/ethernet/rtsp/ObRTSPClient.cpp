// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ObRTSPClient.hpp"
#include "exception/ObException.hpp"

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

#include <chrono>

namespace libobsensor {

#define WAIT_CMD_RESPONES(timeout)                                                                                              \
    {                                                                                                                           \
        if(!commandCv_.wait_for(lk, std::chrono::milliseconds(timeout), [&]() { return commandState_ != CMD_WAITING_RESP; })) { \
            commandState_ = CMD_TIMEOUT;                                                                                        \
        }                                                                                                                       \
    }

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment &operator<<(UsageEnvironment &env, const RTSPClient &rtspClient) {
    return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment &operator<<(UsageEnvironment &env, const MediaSubsession &subsession) {
    return env << subsession.mediumName() << "/" << subsession.codecName();
}

ObRTSPClient *ObRTSPClient::createNew(std::shared_ptr<const StreamProfile> profile, UsageEnvironment &env, char const *rtspURL, MutableFrameCallback callback, int verbosityLevel,
                                      portNumBits tunnelOverHTTPPortNum, int socketNumToServer) {
    return new ObRTSPClient(profile, env, rtspURL, callback, verbosityLevel, tunnelOverHTTPPortNum, socketNumToServer);
}

ObRTSPClient::ObRTSPClient(std::shared_ptr<const StreamProfile> profile, UsageEnvironment &env, char const *rtspURL, MutableFrameCallback callback,
                           int verbosityLevel, portNumBits tunnelOverHTTPPortNum, int socketNumToServer)
    : RTSPClient(env, rtspURL, verbosityLevel, "Open-Orbbec-SDK", tunnelOverHTTPPortNum, socketNumToServer), profile_(profile), frameCallback_(callback) {
    envir() << "ObRTSPClient created! rtspURL = " << url();
}

ObRTSPClient::~ObRTSPClient() noexcept {
    TRY_EXECUTE(stopStream());

    // Ensure that the taskScheduler eventLoop thread is closed before performing the following operations

    clearSinks();  // Closing the stream might cause an exception; sinks need to be cleared again
    if(mediaSession_ != nullptr) {
        // We also need to delete "session", and unschedule "streamTimerTask_" (if set)
        if(streamTimerTask_ != NULL) {
            envir().taskScheduler().unscheduleDelayedTask(streamTimerTask_);
        }
        Medium::close(mediaSession_);
        mediaSession_ = nullptr;
    }
    if(curSubsessionIter_ != nullptr) {
        delete curSubsessionIter_;
        curSubsessionIter_ = nullptr;
    }

    envir() << "ObRTSPClient destructor! rtspURL = " << url();
}

void ObRTSPClient::startStream() {
    std::unique_lock<std::recursive_mutex> lk(commandMutex_);
    commandState_ = CMD_WAITING_RESP;

    DESCRIBE();  // The SETUP and PLAY steps will be completed sequentially in each response function

    WAIT_CMD_RESPONES(RTSP_RESPONSE_TIMEOUT_MS);
    if(commandState_ != CMD_DONE) {
        if(commandState_ == CMD_TIMEOUT && !errorMsg_.length()) {
            errorMsg_ = utils::string::to_string() << "Wait command respones failed! Timeout! state=" << std::to_string(RTSPState_);
        }
        std::string msg = errorMsg_;

        if(RTSPState_ >= RTSP_SETUP) {
            TRY_EXECUTE(stopStream());
        }

        throw libobsensor::camera_disconnected_exception(msg);
    }

    envir() << "ObRTSPClient stream started! rtspURL = " << url() << "\n";
}

void ObRTSPClient::stopStream() {
    if(RTSPState_ == RTSP_PLAY) {
        std::unique_lock<std::recursive_mutex> lk(commandMutex_);
        commandState_ = CMD_WAITING_RESP;

        TEARDOWN();

        WAIT_CMD_RESPONES(RTSP_RESPONSE_TIMEOUT_TEARDOWN_MS);
        if(commandState_ != CMD_DONE) {
            if(commandState_ == CMD_TIMEOUT && !errorMsg_.length()) {
                errorMsg_ = utils::string::to_string() << "Wait command respones failed! Timeout! state=TEARDOWN";
            }
            throw libobsensor::camera_disconnected_exception(errorMsg_);
        }
        envir() << "ObRTSPClient: stream stoped! rtspURL = " << url() << "\n";
    }
}

void ObRTSPClient::DESCRIBE() {
    RTSPState_ = RTSP_DESCRIBE;
    envir() << url() << ": Send describe command to get SDP, to create medea session\n";
    sendDescribeCommand(cmdResponseHandlerDESCRIBE);
}

void ObRTSPClient::cmdResponseHandlerDESCRIBE(RTSPClient *rtspClient, int resultCode, char *resultString) {
    ObRTSPClient *obRtspClient = (ObRTSPClient *)rtspClient;
    if(obRtspClient->RTSPState_ == RTSP_DESCRIBE && obRtspClient->commandState_ != CMD_TIMEOUT) {
        do {
            UsageEnvironment &env = rtspClient->envir();  // alias

            if(resultCode != 0) {
                obRtspClient->commandState_ = CMD_RESP_WITH_ERROR;
                obRtspClient->errorMsg_     = utils::string::to_string() << rtspClient->url() << ": Failed to get a SDP description";
                obRtspClient->commandCv_.notify_all();
                break;
            }

            env << *rtspClient << "Got a SDP description:\n" << resultString;

            // Create a media session object from this SDP description:
            char *const sdpDescription  = resultString;
            obRtspClient->mediaSession_ = MediaSession::createNew(env, sdpDescription);

            if(obRtspClient->mediaSession_ == NULL) {
                obRtspClient->commandState_ = CMD_RESP_WITH_ERROR;
                obRtspClient->errorMsg_     = utils::string::to_string()
                                          << rtspClient->url() << ": Failed to create a MediaSession object from the SDP description: " << env.getResultMsg();
                obRtspClient->commandCv_.notify_all();
                break;
            }
            else if(!obRtspClient->mediaSession_->hasSubsessions()) {
                obRtspClient->commandState_ = CMD_RESP_WITH_ERROR;
                obRtspClient->errorMsg_ = utils::string::to_string() << rtspClient->url() << ": This session has no media subsessions (i.e., no \"m=\" lines)";
                obRtspClient->commandCv_.notify_all();
                break;
            }
            obRtspClient->SETUP();
        } while(0);
    }
    delete[] resultString;
}

void ObRTSPClient::SETUP() {
    RTSPState_         = RTSP_SETUP;
    curSubsessionIter_ = new MediaSubsessionIterator(*mediaSession_);
    setupNextSubsession();
}

void ObRTSPClient::setupNextSubsession() {
    if((curSubsession_ = curSubsessionIter_->next()) != NULL) {
        if(!curSubsession_->initiate()) {
            commandState_ = CMD_RESP_WITH_ERROR;
            errorMsg_     = utils::string::to_string() << "Failed to initiate the \"" << curSubsession_->mediumName() << "/" << curSubsession_->codecName()
                                                   << "\" subsession: " << envir().getResultMsg() << "\n";
            commandCv_.notify_all();
        }
        else {
            envir() << this << "Initiated the \"" << *curSubsession_ << "\" subsession (";
            if(curSubsession_->rtcpIsMuxed()) {
                envir() << "client port " << curSubsession_->clientPortNum();
            }
            else {
                envir() << "client ports " << curSubsession_->clientPortNum() << "-" << curSubsession_->clientPortNum() + 1;
            }
            envir() << ")\n";

            envir() << url() << ": Setup the \"" << *curSubsession_->mediumName() << "/" << *curSubsession_->codecName() << "\" subsession \n";
            sendSetupCommand(*curSubsession_, cmdResponseHandlerSETUP, False, RTP_STREAMING_OVER_TCP);
        }
    }
    else {
        PLAY();
    }
}

void ObRTSPClient::cmdResponseHandlerSETUP(RTSPClient *rtspClient, int resultCode, char *resultString) {
    ObRTSPClient *obRtspClient = (ObRTSPClient *)rtspClient;
    if(obRtspClient->RTSPState_ == RTSP_SETUP && obRtspClient->commandState_ != CMD_TIMEOUT) {
        do {
            UsageEnvironment     &env           = rtspClient->envir();           // alias
            MutableFrameCallback &frameCallback = obRtspClient->frameCallback_;  // alias
            MediaSubsession      *subsession    = obRtspClient->curSubsession_;  // alias
            if(resultCode != 0) {
                obRtspClient->commandState_ = CMD_RESP_WITH_ERROR;
                obRtspClient->errorMsg_     = utils::string::to_string() << rtspClient->url() << ": Failed to set up the \"" << *subsession->mediumName() << "/"
                                                                     << *subsession->codecName() << "\" subsession: " << resultString << "";
                obRtspClient->commandCv_.notify_all();
                break;
            }
            // Having successfully setup the curSubsession, create a data sink for it.
            // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
            // after we've sent a RTSP "PLAY" command.)
            subsession->sink = ObRTPSink::createNew(obRtspClient->profile_, env, *subsession, frameCallback, rtspClient->url());
            if(subsession->sink == NULL) {
                obRtspClient->commandState_ = CMD_RESP_WITH_ERROR;
                obRtspClient->errorMsg_     = utils::string::to_string()
                                          << rtspClient->url() << ": Failed to create a data sink for the \"" << *subsession->mediumName() << "/"
                                          << *subsession->codecName() << "\" subsession: " << env.getResultMsg();
                obRtspClient->commandCv_.notify_all();
                break;
            }
            env << *rtspClient << "Created a data sink for the \"" << *subsession << "\" subsession\n";
            subsession->miscPtr = rtspClient;  // a hack to let curSubsession handler functions get the "RTSPClient" from the curSubsession
            // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
            if(subsession->rtcpInstance() != NULL) {
                subsession->rtcpInstance()->setByeWithReasonHandler(subsessionByeHandler, subsession);
            }
            subsession->sink->startPlaying(*(subsession->readSource()), subsessionAfterPlayingHandler, subsession);
            obRtspClient->setupNextSubsession();
        } while(0);
    }
    delete[] resultString;
}

void ObRTSPClient::PLAY() {
    // PLAY
    RTSPState_ = RTSP_PLAY;

    if(mediaSession_->absStartTime() != NULL) {
        // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
        envir() << url() << ": Play! MediaSession=" << mediaSession_ << "\n";
        sendPlayCommand(*mediaSession_, cmdResponseHandlerPLAY, mediaSession_->absStartTime(), mediaSession_->absEndTime());
        duration_ = 0.0;
    }
    else {
        duration_ = mediaSession_->playEndTime() - mediaSession_->playStartTime();
        envir() << url() << ": Play! MediaSession=" << mediaSession_ << ", duration=" << duration_ << "\n";
        sendPlayCommand(*mediaSession_, cmdResponseHandlerPLAY);
    }
}

void ObRTSPClient::cmdResponseHandlerPLAY(RTSPClient *rtspClient, int resultCode, char *resultString) {
    ObRTSPClient *obRtspClient = (ObRTSPClient *)rtspClient;

    if(obRtspClient->RTSPState_ == RTSP_PLAY && obRtspClient->commandState_ != CMD_TIMEOUT) {
        if(resultCode != 0) {
            obRtspClient->commandState_ = CMD_RESP_WITH_ERROR;
            obRtspClient->errorMsg_     = utils::string::to_string() << "[URL:\"" << rtspClient->url() << "\"]: "
                                                                 << "Failed to start playing session: " << resultString << "\n";
            obRtspClient->commandCv_.notify_all();
        }
        else {
            UsageEnvironment &env = rtspClient->envir();  // alias
            // Set a timer to be handled at the end of the stream's expected duration_ (if the stream does not already signal its end
            // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
            // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
            // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
            if(obRtspClient->duration_ > 0) {
                const double delaySlop = 0.1;  // number of seconds extra to delay, after the stream's expected duration_.  (This is optional.)
                obRtspClient->duration_ += delaySlop;
                unsigned uSecsToDelay          = (unsigned)(obRtspClient->duration_ * 1000000);
                obRtspClient->streamTimerTask_ = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc *)streamStopEventHandler, rtspClient);
            }
            env << *rtspClient << "Started playing session";
            if(obRtspClient->duration_ > 0) {
                env << " (for up to " << obRtspClient->duration_ << " seconds)";
            }
            env << "...\n";
            // Stream opened successfully
            obRtspClient->commandState_ = CMD_DONE;
            obRtspClient->commandCv_.notify_all();
        }
    }
    delete[] resultString;
}

void ObRTSPClient::TEARDOWN() {
    RTSPState_ = RTSP_TEARDOWN;
    envir() << url() << ": Send TEARDOWN command to stop stream\n";
    sendTeardownCommand(*mediaSession_, cmdResponseHandlerTEARDOWN);
}

void ObRTSPClient::cmdResponseHandlerTEARDOWN(RTSPClient *rtspClient, int resultCode, char *resultString) {
    UsageEnvironment &env          = rtspClient->envir();  // alias
    ObRTSPClient     *obRtspClient = (ObRTSPClient *)rtspClient;
    if(obRtspClient->RTSPState_ == RTSP_TEARDOWN && obRtspClient->commandState_ != CMD_TIMEOUT) {
        obRtspClient->commandState_ = CMD_DONE;
        obRtspClient->commandCv_.notify_all();
    }
    if(resultString != nullptr) {
        env << "TEARDOWN response with: " << resultString << ", code=" << resultCode << "\n";
        delete[] resultString;
    }
}

void ObRTSPClient::clearSinks() {
    // Clear the sinks bound to each subsession
    if(mediaSession_ != nullptr) {
        MediaSubsessionIterator subsessionIter(*mediaSession_);
        MediaSubsession        *subsession;

        while((subsession = subsessionIter.next()) != NULL) {
            if(subsession->sink != NULL) {
                Medium::close(subsession->sink);  // Stop event scheduling; the sink will be destructed, and stopPlaying will be called during its destruction
                subsession->sink = NULL;

                if(subsession->rtcpInstance() != NULL) {
                    subsession->rtcpInstance()->setByeHandler(NULL, NULL);  // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
                }
            }
        }
    }
}

void ObRTSPClient::subsessionAfterPlayingHandler(void *clientData) {
    MediaSubsession *subsession   = (MediaSubsession *)clientData;
    ObRTSPClient    *obRtspClient = (ObRTSPClient *)(subsession->miscPtr);
    if(obRtspClient->RTSPState_ != RTSP_TEARDOWN) {
        TRY_EXECUTE(obRtspClient->stopStream());
    }
}

void ObRTSPClient::subsessionByeHandler(void *clientData, char const *reason) {
    MediaSubsession  *subsession   = (MediaSubsession *)clientData;
    ObRTSPClient     *obRtspClient = (ObRTSPClient *)subsession->miscPtr;
    UsageEnvironment &env          = obRtspClient->envir();  // alias

    env << *obRtspClient << "Received RTCP \"BYE\"";
    if(reason != NULL) {
        env << " (reason:\"" << reason << "\")";
        delete[](char *)reason;
    }
    env << " on \"" << *subsession << "\" subsession\n";

    // Begin by closing this subsession's stream:
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession           &session = subsession->parentSession();
    MediaSubsessionIterator subsessionIter(session);
    while((subsession = subsessionIter.next()) != NULL) {
        if(subsession->sink != NULL)
            return;  // this subsession is still active
    }

    {
        if(obRtspClient->RTSPState_ == RTSP_PLAY && obRtspClient->commandState_ == CMD_WAITING_RESP) {
            obRtspClient->errorMsg_     = "RTSP-PLAY Failed! Received RTCP\"BYE\"";
            obRtspClient->commandState_ = CMD_RESP_WITH_ERROR;
            obRtspClient->commandCv_.notify_all();
        }
    }

    if(obRtspClient->RTSPState_ != RTSP_TEARDOWN) {
        TRY_EXECUTE(obRtspClient->stopStream());
    }
}

void ObRTSPClient::streamStopEventHandler(void *clientData) {
    auto rtspClient = (ObRTSPClient *)clientData;
    rtspClient->envir() << "Stream stop event occur";
    rtspClient->streamTimerTask_ = NULL;
    // stop the stream:
    TRY_EXECUTE(rtspClient->stopStream());
}

}  // namespace libobsensor

