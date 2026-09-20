import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class StdUint32MappingTest(unittest.TestCase):
    def test_power_registers_and_legacy_addresses(self):
        mapping_text = (
            ROOT / "modbus_mappings/modbus_default_mapping0.json"
        ).read_text()
        # Mapping source files are comma-separated recipe fragments rather than
        # standalone JSON arrays.
        mapping = json.loads(f"[{mapping_text}]")
        by_register = {item["register"]: item for item in mapping}

        expected = {
            14: ("power_delivered_kw", "uint32"),
            16: ("power_returned_kw", "uint32"),
            38: ("net_power_l1_kw", "int32"),
            40: ("net_power_l2_kw", "int32"),
            42: ("net_power_l3_kw", "int32"),
            46: ("p1_device_id", "uint32"),
            48: ("net_power_total_kw", "int32"),
            50: ("power_delivered_l1_kw", "uint32"),
            52: ("power_delivered_l2_kw", "uint32"),
            54: ("power_delivered_l3_kw", "uint32"),
            56: ("power_returned_l1_kw", "uint32"),
            58: ("power_returned_l2_kw", "uint32"),
            60: ("power_returned_l3_kw", "uint32"),
        }
        for register, (source, data_type) in expected.items():
            self.assertEqual(by_register[register]["source"], source)
            self.assertEqual(by_register[register]["type"], data_type)
            if register != 46:
                self.assertEqual(by_register[register]["scale"], 1000)

    def test_generated_table_matches_json_power_extension(self):
        table = (ROOT / "_mbus_mapping.h").read_text()
        for register, source in {
            48: "net_power_total_kw",
            50: "power_delivered_l1_kw",
            52: "power_delivered_l2_kw",
            54: "power_delivered_l3_kw",
            56: "power_returned_l1_kw",
            58: "power_returned_l2_kw",
            60: "power_returned_l3_kw",
        }.items():
            pattern = rf"\{{{register},\s+1000,\s+\(uint8_t\)MbSource::{source},"
            self.assertRegex(table, re.compile(pattern))

    def test_sdm630_total_active_energy_register(self):
        table = (ROOT / "_mbus_mapping.h").read_text()
        pattern = (
            r"\{342,\s+1,\s+\(uint8_t\)MbSource::energy_total_abs_kwh,"
            r"\s+\(uint8_t\)ModbusDataType::FLOAT,\s+0\}"
        )
        self.assertRegex(table, re.compile(pattern))

    def test_sdm630_extended_measurements(self):
        table = (ROOT / "_mbus_mapping.h").read_text()
        expected = {
            18: (1, "apparent_power_l1_va"),
            20: (1, "apparent_power_l2_va"),
            22: (1, "apparent_power_l3_va"),
            48: (1, "current_total_a"),
            56: (1, "apparent_power_total_va"),
        }
        for register, (scale, source) in expected.items():
            pattern = (
                rf"\{{{register},\s+{scale},\s+\(uint8_t\)MbSource::{source},\s*"
                rf"\(uint8_t\)ModbusDataType::FLOAT,\s+0\}}"
            )
            self.assertRegex(table, re.compile(pattern))

    def test_sdm630_reactive_power_is_zero(self):
        table = (ROOT / "_mbus_mapping.h").read_text()
        for register in (24, 26, 28, 60):
            pattern = (
                rf"\{{{register},\s+1,\s+\(uint8_t\)MbSource::constant,"
                rf"\s+\(uint8_t\)ModbusDataType::FLOAT,\s+0\}}"
            )
            self.assertRegex(table, re.compile(pattern))

    def test_sdm630_zero_fill_is_profile_specific(self):
        implementation = (ROOT / "_mbus.ino").read_text()
        self.assertIn("activeRecipeZeroFill = (mappingChoice == 1);", implementation)
        self.assertIn("activeRecipeZeroFill ? 0U : MBUS_VAL_UNAVAILABLE", implementation)


if __name__ == "__main__":
    unittest.main()
