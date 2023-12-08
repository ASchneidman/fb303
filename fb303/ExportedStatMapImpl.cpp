/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fb303/ExportedStatMapImpl.h>
#include <fb303/TimeseriesExporter.h>

using folly::StringPiece;

namespace facebook::fb303 {

void ExportedStatMapImpl::exportStat(
    LockableStat stat,
    folly::StringPiece name,
    ExportType type) {
  return exportStat(std::move(stat), name, type, true /* updateOnRead */);
}

void ExportedStatMapImpl::exportStat(
    LockableStat stat,
    StringPiece name,
    ExportType exportType,
    bool updateOnRead) {
  StatPtr item = stat.getStatPtr();
  TimeseriesExporter::exportStat(
      item, exportType, name, dynamicCounters_, updateOnRead);
}

ExportedStatMapImpl::LockableStat ExportedStatMapImpl::getLockableStat(
    StringPiece name,
    const ExportType* type) {
  return ExportedStatMapImpl::LockableStat(
      ExportedStatMap::getStatPtr(name, type));
}

ExportedStatMapImpl::LockableStat ExportedStatMapImpl::getLockableStat(
    folly::StringPiece name,
    folly::Range<const ExportType*> exportTypes) {
  return ExportedStatMapImpl::LockableStat(
      ExportedStatMap::getStatPtr(name, exportTypes));
}

ExportedStatMapImpl::LockableStat ExportedStatMapImpl::getLockableStatNoExport(
    StringPiece name,
    bool* createdPtr,
    const ExportedStat* copyMe) {
  return ExportedStatMapImpl::LockableStat(
      ExportedStatMap::getStatPtrNoExport(name, createdPtr, copyMe));
}

} // namespace facebook::fb303
