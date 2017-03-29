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

#ifndef CHRE_CORE_WWAN_REQUEST_MANAGER_H_
#define CHRE_CORE_WWAN_REQUEST_MANAGER_H_

#include <cstdint>

#include "chre/core/nanoapp.h"
#include "chre/platform/platform_wwan.h"
#include "chre/util/non_copyable.h"
#include "chre/util/optional.h"

namespace chre {

/**
 * The WwanRequestManager handles requests from nanoapps for WWAN data. This
 * includes multiplexing multiple requests into one for the platform to handle.
 *
 * This class is effectively a singleton as there can only be one instance of
 * the PlatformWwan instance.
 */
class WwanRequestManager : public NonCopyable {
 public:
  /**
   * @return the WWAN capabilities exposed by this platform.
   */
  uint32_t getCapabilities();

  /**
   * Performs a request for cell neighbor info for the given nanoapp.
   *
   * @param nanoapp The nanoapp requesting the cell info.
   * @param cookie A cookie provided by the nanoapp to supply context in the
   *        asynchronous result event.
   * @return true if the request was accepted.
   */
  bool requestCellInfo(Nanoapp *nanoapp, const void *cookie);

 private:
  //! The instance of the platform WWAN interface.
  PlatformWwan mPlatformWwan;

  // TODO: Support multiple requests for cell info by enqueuing them and
  // requesting one after another.
  //! The nanoapp that is currently requesting cell info. At this time only one
  //! nanoapp can have a pending request for cell info.
  Optional<uint32_t> mCellInfoRequestingNanoappInstanceId;

  //! The cookie passed in by a nanoapp making a request for cell info. Note
  //! that this will only be valid if the mCellInfoRequestingNanoappInstanceId
  //! is set.
  const void *mCellInfoRequestingNanoappCookie;
};

}  // namespace chre

#endif  // CHRE_CORE_WWAN_REQUEST_MANAGER_H_
