/*
  Retrieves configuration values, set via configuration file or using
  default values otherwise.

  Copyright (c) 2016 - 2020 SUSE LLC

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Configuration.h"
#include "Util.h"
#include <map>
#include <stdexcept>

Configuration::Configuration() {
    econf_err error = econf_newIniFile(&key_file);
    if (error)
        throw runtime_error{"Could not create default configuration."};
    map<const char*, const char*> defaults = {
            {"LOCKFILE", "/var/run/transactional-update.pid"}
    };
    for(auto &e : defaults) {
        error = econf_setStringValue(key_file, "", e.first, e.second);
        if (error)
            throw runtime_error{"Could not set default value for '" + string(e.first) + "'."};
    }
}

Configuration::~Configuration() {
    econf_freeFile(key_file);
}

string Configuration::get(const string &key) {
    CString val;
    econf_err error = econf_getStringValue(key_file, "", key.c_str(), &val.ptr);
    if (error)
        throw runtime_error{"Could not read configuration setting '" + key + "'."};
    return string(val);
}