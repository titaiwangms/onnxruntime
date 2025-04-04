// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/platform/apple/logging/apple_log_sink.h"

#import <Foundation/Foundation.h>

#include <sstream>

namespace onnxruntime {
namespace logging {

void AppleLogSink::SendImpl(const Timestamp& timestamp, const std::string& logger_id, const Capture& message) {
  using timestamp_ns::operator<<;
  std::ostringstream msg;

  timestamp_ns::operator<<(msg, timestamp);  // handle ambiguity with C++20 where date and std::chrono have operator<<
  msg << " [" << message.SeverityPrefix() << ":" << message.Category() << ":" << logger_id << ", "
      << message.Location().ToString() << "] " << message.Message();
  NSLog(@"%s", msg.str().c_str());
}

}  // namespace logging
}  // namespace onnxruntime
