/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>

#include "qurt.h"

#include "chre/core/event_loop_manager.h"
#include "chre/core/host_comms_manager.h"
#include "chre/platform/log.h"
#include "chre/platform/slpi/fastrpc.h"
#include "chre/util/fixed_size_blocking_queue.h"

namespace chre {

namespace {

constexpr size_t kOutboundQueueSize = 32;
FixedSizeBlockingQueue<const MessageToHost *, kOutboundQueueSize>
    gOutboundQueue;

/**
 * FastRPC method invoked by the host to block on messages
 *
 * @param buffer Output buffer to populate with message data
 * @param bufferLen Size of the buffer, in bytes
 * @param messageLen Output parameter to populate with the size of the message
 *        in bytes upon success
 *
 * @return 0 on success, nonzero on failure
 */
extern "C" int chre_slpi_get_message_to_host(
    unsigned char *buffer, int bufferLen, unsigned int *messageLen) {
  CHRE_ASSERT(buffer != nullptr);
  CHRE_ASSERT(bufferLen > 0);
  CHRE_ASSERT(messageLen != nullptr);

  const MessageToHost *message = gOutboundQueue.pop();

  int result;
  if (message == nullptr) {
    // A null message is used during shutdown so the calling thread can exit
    result = CHRE_FASTRPC_ERROR_SHUTTING_DOWN;
  } else {
    if (bufferLen <= 0
        || message->message.size() > INT_MAX
        || message->message.size() > static_cast<size_t>(bufferLen)) {
      // Note that we can't use regular logs here as they can result in sending
      // a message, leading to an infinite loop if the error is persistent
      FARF(FATAL, "Invalid buffer size %d or message size %zu", bufferLen,
           message->message.size());
      result = CHRE_FASTRPC_ERROR;
    } else {
      // TODO: wrap in a FlatBuffer or some other structure so we pass metadata
      LOGD("Copying message of size %zu", message->message.size());
      memcpy(buffer, message->message.data(), message->message.size());
      *messageLen = message->message.size();
      result = CHRE_FASTRPC_SUCCESS;
    }

    auto& hostCommsManager =
        EventLoopManagerSingleton::get()->getHostCommsManager();
    hostCommsManager.onMessageToHostComplete(message);
  }

  return result;
}

/**
 * FastRPC method invoked by the host to send a message to the system
 *
 * @param buffer
 * @param size
 *
 * @return 0 on success, nonzero on failure
 */
extern "C" int chre_slpi_deliver_message_from_host(const unsigned char *message,
                                                   int messageLen) {
  // TODO: implement
  return CHRE_FASTRPC_SUCCESS;
}

}  // anonymous namespace

bool HostLink::sendMessage(const MessageToHost *message) {
  return gOutboundQueue.push(message);
}

void HostLinkBase::shutdown() {
  constexpr qurt_timer_duration_t kPollingIntervalUsec = 5000;

  // Push a null message so the blocking call in chre_slpi_get_message_to_host()
  // returns and the host can exit cleanly. If the queue is full, try again to
  // avoid getting stuck (no other new messages should be entering the queue at
  // this time). Don't wait too long as the host-side binary may have died in
  // a state where it's not blocked in chre_slpi_get_message_to_host().
  int retryCount = 5;
  FARF(MEDIUM, "Shutting down host link");
  while (!gOutboundQueue.push(nullptr) && --retryCount > 0) {
    qurt_timer_sleep(kPollingIntervalUsec);
  }

  if (retryCount <= 0) {
    // Don't use LOGE, as it may involve trying to send a message
    FARF(ERROR, "No room in outbound queue for shutdown message and host not "
         "draining queue!");
  } else {
    FARF(MEDIUM, "Draining message queue");

    // We were able to push the shutdown message. Wait for the queue to
    // completely flush before returning.
    int waitCount = 5;
    while (!gOutboundQueue.empty() && --waitCount > 0) {
      qurt_timer_sleep(kPollingIntervalUsec);
    }

    if (waitCount <= 0) {
      FARF(ERROR, "Host took too long to drain outbound queue; exiting anyway");
    } else {
      FARF(MEDIUM, "Finished draining queue");
    }
  }
}

}  // namespace chre
