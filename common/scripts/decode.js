function decodeUplink(input) {
        var ret = {
                data: 0,
        }

        switch (input.fPort) {
                case 0x00:
                        ret.data = decodeKitData(input.bytes)
                        break
                case 0x10:
                        ret.data = decodeRenogySystemConfig(input.bytes)
                        break
                case 0x11:
                        ret.data = decodeRenogyBatteryConfig(input.bytes)
                        break
                case 0x12:
                        ret.data = decodeRenogyChargingState(input.bytes)
                        break
                case 0x13:
                        ret.data = decodeRenogyStatus(input.bytes)
                        break
                case 0x20:
                        ret.data = decodeBootStatus(input.bytes)
                        break;
                case 0x80:
                case 0x81:
                case 0x82:
                        /* Outputs */
                        ret.data = { state: input.bytes[0] }
                        break;
                case 200:
                        ret.data = decodeMCastResponse(input.bytes)
                        break;
                case 201:
                        ret.data = decodeFragResponse(input.bytes)
                        break;
                default:
                        break
                }

        return ret
}

function getU32(index, bytes) {
        return (
                bytes[index+0] << 0 |
                bytes[index+1] << 8 |
                bytes[index+2] << 16 |
                bytes[index+3] << 24
        )
}

function getU16(index, bytes) {
        return (
                bytes[index+0] << 0 |
                bytes[index+1] << 8
        )
}

function getString(index, len, bytes) {
        return String.fromCharCode(...bytes.slice(index, index + len))
}

function getVersion(index, bytes) {
        var major = bytes[index]
        var minor = bytes[index]
        var fix = bytes[index]
        return major + "." + minor + "." + fix
}

function decodeBootStatus(bytes) {
        return {
                reset_reason: getU32(0, bytes),
                version: getString(4, 32, bytes),
        }
}

function decodeMCastResponse(bytes) {
        /* Response type is first byte */
        switch (bytes[0]) {
                case 0x00:
                        return {
                                type: "pkg_version",
                                pkg_id: bytes[1],
                                frag_transport_version: bytes[2],
                        }
                case 0x02:
                        return {
                                type: "mcast_setup",
                                mcast_id: (bytes[1] & 0x03),
                                err: (bytes[1] >> 2),
                        }
                case 0x03:
                        return {
                                type: "mcast_delete",
                                mcast_id: (bytes[1] & 0x03),
                                err: (bytes[1] >> 2),
                        }
                case 0x04:
                        return {
                                type: "mcast_class_c",
                                mcast_status: (bytes[1] & 0x1F),
                                err: (bytes[1] >> 5),
                        }
        }
}

function decodeFragResponse(bytes) {
        /* Response type is first byte */
        switch (bytes[0]) {
                case 0x00:
                        return {
                                type: "pkg_version",
                                pkg_id: bytes[1],
                                frag_transport_version: bytes[2],
                        }
                case 0x01:
                        var nb_frag_rx = getU16(1, bytes)
                        var idx = nb_frag_rx >> 6
                        nb_frag_rx &= 0xCFFF
                        return {
                                type: "frag_status",
                                nb_frag_received: nb_frag_rx,
                                index: idx,
                                nb_frag_missing: bytes[3],
                                status: bytes[4],
                        }
                case 0x02:
                        return {
                                type: "session_setup",
                                status: bytes[1],
                        }
                case 0x03:
                        return {
                                type: "session_delete",
                                status: bytes[1],
                        }
                default:
                        return {
                                type: "unknown"
                        }
        }
}

function decodeRenogySystemConfig(bytes) {
        // renogy_sys_t
        return {
                max_charge_current: bytes[0],
                max_supported_voltage: bytes[1],
                product_type: bytes[2],
                max_discharge_current: bytes[3],
                model: getString(4, 16, bytes),
                software_version: getVersion(20, bytes),
                hardware_version: getVersion(24, bytes),
                serial: getU32(28, bytes),
                device_address: getU16(32, bytes),
        }
}

function decodeRenogyBatteryConfig(bytes) {
        // renogy_param_bat_t
        var ret = {
                detected_battery_V: bytes[0],
                configured_battery_V: bytes[1],
                input_overvolt_V: getU16(2, bytes) / 10.0, // 200
                overvoltage_disconnect_V: getU16(4, bytes) / 10.0, // 160
                battery_type: "",
                battery_type_n: getU16(6, bytes),

                boost_V: getU16(8, bytes) / 10.0, // 14.2
                overvoltage_recovery_V: getU16(10, bytes) / 10.0, // 15.5
                float_V: getU16(12, bytes) / 10.0, // 14.2
                equalization_V: getU16(14, bytes) / 10.0, // 14.2
                over_discharge_recovery_V: getU16(16, bytes) / 10.0, // 12.5
                boost_recovery_V: getU16(18, bytes) / 10.0, // 13.2
                over_discharge_V: getU16(20, bytes) / 10.0, // 11
                undervolt_warn_V: getU16(22, bytes) / 10.0, // 12

                end_discharge_soc: bytes[24],
                end_charge_soc: bytes[25],

                over_discharge_limit_V: getU16(26, bytes) / 10.0, // 10.8
                equalization_charge_s: getU16(28, bytes), // 120
                over_discharge_delay_s: getU16(30, bytes), // 5
                equalization_interval_d: getU16(32, bytes), // 30 (eq days?)
                boost_charge_s: getU16(34, bytes), // 120
        }

        /* Battery type */
        switch (getU16(6, bytes)) {
                case 0:
                        ret.battery_type = "open"
                        break;
                case 1:
                        ret.battery_type = "flooded"
                        break;
                case 2:
                        ret.battery_type = "sealed"
                        break;
                case 2:
                        ret.battery_type = "gel"
                        break;
                case 3:
                        ret.battery_type = "lithium"
                        break;
                case 4:
                        ret.battery_type = "custom"
                        break;
                default:
                        ret.battery_type = "unknown"
                        break;
        }

        return ret
}

function decodeRenogyChargingState(bytes) {
        // renogy_dyn_status_t
        var ret = {
                charge_state: bytes[0],
                load_on: (bytes[1] & 0x80) != 0,
                load_dim: (bytes[1] & 0x7F),
        }

        /* Charge state */
        switch (bytes[0]) {
                case 0:
                        ret.charge_state = "deactivated"
                        break;
                case 1:
                        ret.charge_state = "activated"
                        break;
                case 2:
                        ret.charge_state = "MPPT"
                        break;
                case 3:
                        ret.charge_state = "equalizing"
                        break;
                case 4:
                        ret.charge_state = "boost"
                        break;
                case 5:
                        ret.charge_state = "float"
                        break;
                case 6:
                        ret.charge_state = "overpower"
                        break;
                case 7:
                        ret.charge_state = "unknown"
        }

        return ret
}

function decodeRenogyStatus(bytes) {
        // renogy_dyn_statistics_t
        return {
                soc_pct: getU16(0, bytes),
                battery_V: getU16(2, bytes) / 10.0,
                charge_A: getU16(4, bytes) / 100.0,
                controller_temp_C: bytes[7],
                battery_temp_C: bytes[6],
                load_voltage_V: getU16(8, bytes) / 10.0,
                load_current_A: getU16(10, bytes) / 100.0,
                load_power_W: getU16(12, bytes),
                solar_voltage_V: getU16(14, bytes) / 10.0,
                solar_current_A: getU16(16, bytes) / 100.0,
                solar_power_W: getU16(18, bytes),
        }
}

function decodeKitData(bytes) {
        // init
        var bytesString = bytes2HexString(bytes)
                .toLocaleUpperCase();
        var decoded = {
                // valid
                valid: true,
                // bytes
                payload: bytesString,
                // messages array
                messages: []
        };

        // CRC check
        if (!crc16Check(bytesString)) {
                decoded["valid"] = false;
                decoded["err"] = "crc check fail."
                return decoded
        }

        // Length Check
        if ((((bytesString.length / 2) - 2) % 7) !== 0) {
                decoded["valid"] = false;
                decoded["err"] = "length check fail."
                return decoded
        }

        // Cache sensor id
        var sensorEuiLowBytes;
        var sensorEuiHighBytes;

        // Handle each frame
        var frameArray = divideBy7Bytes(bytesString);
        for (var forFrame = 0; forFrame < frameArray.length; forFrame++) {
                var frame = frameArray[forFrame];
                // Extract key parameters
                var channel = strTo10SysNub(frame.substring(0, 2));
                var dataID = strTo10SysNub(frame.substring(2, 6));
                var dataValue = frame.substring(6, 14);
                var realDataValue = isSpecialDataId(dataID) ? ttnDataSpecialFormat(dataID, dataValue) : ttnDataFormat(dataValue);

                if (checkDataIdIsMeasureUpload(dataID)) {
                        // if telemetry.
                        decoded.messages.push({
                                type: "report_telemetry",
                                measurementId: dataID,
                                measurementValue: realDataValue
                        });
                } else if (isSpecialDataId(dataID) || (dataID === 5) || (dataID === 6)) {
                        // if special order, except "report_sensor_id".
                        switch (dataID) {
                                case 0x00:
                                        // node version
                                        var versionData = sensorAttrForVersion(realDataValue);
                                        decoded.messages.push({
                                                type: "upload_version",
                                                hardwareVersion: versionData.ver_hardware,
                                                softwareVersion: versionData.ver_software
                                        });
                                        break;
                                case 1:
                                        // sensor version
                                        break;
                                case 2:
                                        // sensor eui, low bytes
                                        sensorEuiLowBytes = realDataValue;
                                        break;
                                case 3:
                                        // sensor eui, high bytes
                                        sensorEuiHighBytes = realDataValue;
                                        break;
                                case 7:
                                        // battery power && interval
                                        decoded.messages.push({
                                                type: "upload_battery",
                                                battery: realDataValue.power
                                        }, {
                                                type: "upload_interval",
                                                interval: parseInt(realDataValue.interval) * 60
                                        });
                                        break;
                                case 0x120:
                                        // remove sensor
                                        decoded.messages.push({
                                                type: "report_remove_sensor",
                                                channel: 1
                                        });
                                        break;
                                default:
                                        break;
                        }
                } else {
                        decoded.messages.push({
                                type: "unknown_message",
                                dataID: dataID,
                                dataValue: dataValue
                        });
                }

        }

        // if the complete id received, as "upload_sensor_id"
        if (sensorEuiHighBytes && sensorEuiLowBytes) {
                decoded.messages.unshift({
                        type: "upload_sensor_id",
                        channel: 1,
                        sensorId: (sensorEuiHighBytes + sensorEuiLowBytes).toUpperCase()
                });
        }

        // return
        return decoded
}

function crc16Check(data) {
        var crc16tab = [
                0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
                0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
                0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
                0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
                0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
                0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
                0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
                0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
                0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
                0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
                0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
                0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
                0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
                0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
                0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
                0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
                0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
                0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
                0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
                0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
                0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
                0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
                0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
                0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
                0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
                0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
                0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
                0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
                0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
                0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
                0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
                0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
        ];
        var result = false;
        var crc = 0;
        var dataArray = [];
        for (var i = 0; i < data.length; i += 2) {
                dataArray.push(data.substring(i, i + 2));
        }

        for (var j = 0; j < dataArray.length; j++) {
                var item = dataArray[j];
                crc = (crc >> 8) ^ crc16tab[(crc ^ parseInt(item, 16)) & 0xFF];
        }
        if (crc === 0) {
                result = true;
        }
        return result;
}

// util
function bytes2HexString(arrBytes) {
        var str = "";
        for (var i = 0; i < arrBytes.length; i++) {
                var tmp;
                var num = arrBytes[i];
                if (num < 0) {
                        tmp = (255 + num + 1).toString(16);
                } else {
                        tmp = num.toString(16);
                }
                if (tmp.length === 1) {
                        tmp = "0" + tmp;
                }
                str += tmp;
        }
        return str;
}

// util
function divideBy7Bytes(str) {
        var frameArray = [];
        for (var i = 0; i < str.length - 4; i += 14) {
                var data = str.substring(i, i + 14);
                frameArray.push(data);
        }
        return frameArray;
}

// util
function littleEndianTransform(data) {
        var dataArray = [];
        for (var i = 0; i < data.length; i += 2) {
                dataArray.push(data.substring(i, i + 2));
        }
        dataArray.reverse();
        return dataArray;
}

// util
function strTo10SysNub(str) {
        var arr = littleEndianTransform(str);
        return parseInt(arr.toString()
                .replace(/,/g, ""), 16);
}

// util
function checkDataIdIsMeasureUpload(dataId) {
        return parseInt(dataId) > 4096;
}

// configurable.
function isSpecialDataId(dataID) {
        switch (dataID) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 7:
                case 0x120:
                        return true;
                default:
                        return false;
        }
}

// configurable
function ttnDataSpecialFormat(dataId, str) {
        var strReverse = littleEndianTransform(str);
        if (dataId === 2 || dataId === 3) {
                return strReverse.join("");
        }

        // handle unsigned number
        var str2 = toBinary(strReverse);

        var dataArray = [];
        switch (dataId) {
                case 0: // DATA_BOARD_VERSION
                case 1: // DATA_SENSOR_VERSION
                        // Using point segmentation
                        for (var k = 0; k < str2.length; k += 16) {
                                var tmp146 = str2.substring(k, k + 16);
                                tmp146 = (parseInt(tmp146.substring(0, 8), 2) || 0) + "." + (parseInt(tmp146.substring(8, 16), 2) || 0);
                                dataArray.push(tmp146);
                        }
                        return dataArray.join(",");
                case 4:
                        for (var i = 0; i < str2.length; i += 8) {
                                var item = parseInt(str2.substring(i, i + 8), 2);
                                if (item < 10) {
                                        item = "0" + item.toString();
                                } else {
                                        item = item.toString();
                                }
                                dataArray.push(item);
                        }
                        return dataArray.join("");
                case 7:
                        // battery && interval
                        return {
                                interval: parseInt(str2.substr(0, 16), 2),
                                power: parseInt(str2.substr(-16, 16), 2)
                        };
        }
}

// util
function ttnDataFormat(str) {
        var strReverse = littleEndianTransform(str);
        var str2 = toBinary(strReverse);
        if (str2.substring(0, 1) === "1") {
                var arr = str2.split("");
                var reverseArr = [];
                for (var forArr = 0; forArr < arr.length; forArr++) {
                        var item = arr[forArr];
                        if (parseInt(item) === 1) {
                                reverseArr.push(0);
                        } else {
                                reverseArr.push(1);
                        }
                }
                str2 = parseInt(reverseArr.join(""), 2) + 1;
                return "-" + str2 / 1000;
        }
        return parseInt(str2, 2) / 1000;
}

// util
function sensorAttrForVersion(dataValue) {
        var dataValueSplitArray = dataValue.split(",");
        return {
                ver_hardware: dataValueSplitArray[0],
                ver_software: dataValueSplitArray[1]
        };
}

// util
function toBinary(arr) {
        var binaryData = [];
        for (var forArr = 0; forArr < arr.length; forArr++) {
                var item = arr[forArr];
                var data = parseInt(item, 16)
                        .toString(2);
                var dataLength = data.length;
                if (data.length !== 8) {
                        for (var i = 0; i < 8 - dataLength; i++) {
                                data = "0" + data;
                        }
                }
                binaryData.push(data);
        }
        return binaryData.toString()
                .replace(/,/g, "");
}
