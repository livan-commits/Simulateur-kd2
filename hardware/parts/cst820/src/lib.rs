//! CST820-family capacitive touch controller used by the Kode Dot DVT.
//!
//! The transaction shape and register accesses follow the KodeOS driver at the
//! SDK revision pinned by KodeOS. The DVT identity bytes (B7/28/C3) are the
//! values recorded in that SDK's driver header as bench observations; they
//! are not presented as a universal CST820 datasheet default.
//!
//! This model covers the driver's I2C bring-up, identity, six-byte touch frame,
//! configuration register read/write path, deep-sleep NACK and board reset.
//! It does not model capacitive sensing, analog behaviour, scan timing or the
//! electrical INT pulse. The board routes touch INT through TCA6408A P6;
//! page-level integration must signal that expander input separately.

use esp_part::prelude::*;

const SCL: &str = "SCL";
const SDA: &str = "SDA";
const RESET: &str = "RESET";
const ADDRESS: u32 = 0x15;
const REG_FRAME: u8 = 0x01;
const REG_CHIP_ID: u8 = 0xA7;
const REG_SLEEP: u8 = 0xE5;
const SLEEP_DEEP: u8 = 0x03;
const REG_CFG_START: u8 = 0xEC;
const REG_CFG_END: u8 = 0xFE;
const CHIP_ID: u8 = 0xB7;
const PROJECT_ID: u8 = 0x28;
const FIRMWARE_VERSION: u8 = 0xC3;

#[derive(Debug)]
struct Cst820 {
    scl: Option<u32>,
    sda: Option<u32>,
    reset: Option<u32>,
    pointer: u8,
    registers: [u8; 256],
    x: u16,
    y: u16,
    fingers: u8,
    gesture: u8,
    event: u8,
    sleeping: bool,
}

impl Default for Cst820 {
    fn default() -> Self {
        let mut registers = [0; 256];
        registers[REG_CHIP_ID as usize] = CHIP_ID;
        registers[(REG_CHIP_ID + 1) as usize] = PROJECT_ID;
        registers[(REG_CHIP_ID + 2) as usize] = FIRMWARE_VERSION;
        Self {
            scl: None,
            sda: None,
            reset: None,
            pointer: REG_FRAME,
            registers,
            x: 0,
            y: 0,
            fingers: 0,
            gesture: 0,
            event: 0,
            sleeping: false,
        }
    }
}

impl Cst820 {
    fn reset_controller(&mut self) {
        self.pointer = REG_FRAME;
        self.registers = [0; 256];
        self.registers[REG_CHIP_ID as usize] = CHIP_ID;
        self.registers[(REG_CHIP_ID + 1) as usize] = PROJECT_ID;
        self.registers[(REG_CHIP_ID + 2) as usize] = FIRMWARE_VERSION;
        self.x = 0;
        self.y = 0;
        self.fingers = 0;
        self.gesture = 0;
        self.event = 0;
        self.sleeping = false;
    }

    fn frame_byte(&self, register: u8) -> Option<u8> {
        match register {
            0x01 => Some(self.gesture),
            0x02 => Some(self.fingers),
            0x03 => Some(((self.event & 0x03) << 6) | ((self.x >> 8) as u8 & 0x0F)),
            0x04 => Some(self.x as u8),
            0x05 => Some((self.y >> 8) as u8 & 0x0F),
            0x06 => Some(self.y as u8),
            _ => None,
        }
    }

    fn read_register(&self, register: u8) -> u8 {
        self.frame_byte(register).unwrap_or(self.registers[register as usize])
    }

    fn write_register(&mut self, register: u8, value: u8) {
        // Frame and identity bytes are read-only in the KodeOS driver contract.
        if (0x01..=0x06).contains(&register)
            || (REG_CHIP_ID..=REG_CHIP_ID + 2).contains(&register)
        {
            return;
        }
        self.registers[register as usize] = value;
        if register == REG_SLEEP && value == SLEEP_DEEP {
            self.sleeping = true;
        }
    }

    fn read_input_point(name: &str, prefix: &str) -> Option<(u16, u16)> {
        let value = name.strip_prefix(prefix)?;
        let (x, y) = value.split_once(',')?;
        let x = x.parse::<u16>().ok()?.min(4095);
        let y = y.parse::<u16>().ok()?.min(4095);
        Some((x, y))
    }

    fn set_point(&mut self, x: u16, y: u16, event: u8) {
        self.x = x.min(4095);
        self.y = y.min(4095);
        self.fingers = 1;
        self.event = event & 0x03;
    }

    fn bus(&mut self, request: &BusRequest) -> Option<BusReply> {
        let i2c = match request.op.as_ref()? {
            bus_request::Op::I2c(i2c) => i2c,
            _ => return None,
        };
        let mut reply = BusReply {
            correlation_id: request.correlation_id,
            ack: false,
            op: BusOp::I2c as i32,
            ..Default::default()
        };

        if i2c.addr != ADDRESS || self.sleeping {
            return Some(reply);
        }

        if !i2c.write.is_empty() {
            self.pointer = i2c.write[0];
            for value in i2c.write.iter().skip(1) {
                self.write_register(self.pointer, *value);
                self.pointer = self.pointer.wrapping_add(1);
            }
        }

        if i2c.read_len > 0 {
            let mut data = Vec::with_capacity(i2c.read_len as usize);
            for _ in 0..i2c.read_len {
                data.push(self.read_register(self.pointer));
                self.pointer = self.pointer.wrapping_add(1);
            }
            reply.data = data;
        }
        reply.ack = true;
        Some(reply)
    }
}

impl Device for Cst820 {
    const NAME: &'static str = "kode_dot_cst820";
    const ON_BUS: bool = true;

    fn configure(&mut self, config: &PartConfig) {
        self.scl = config.pins.get(SCL).copied();
        self.sda = config.pins.get(SDA).copied();
        self.reset = config.pins.get(RESET).copied();
    }

    fn claims(&self) -> Vec<Claim> {
        let mut claims = vec![Claim {
            what: Some(claim::What::I2c(I2cClaim {
                addr: ADDRESS,
                scl_pin: self.scl,
                sda_pin: self.sda,
            })),
        }];
        if let Some(pin) = self.reset {
            claims.push(watches(1u64 << pin));
        }
        claims
    }

    fn on_pins(&mut self, levels: u64, _now_ns: u64) {
        // The Kode Dot's shared display/touch reset is active low.
        if let Some(pin) = self.reset {
            if levels & (1u64 << pin) == 0 {
                self.reset_controller();
            }
        }
    }

    fn on_bus(&mut self, request: &BusRequest) -> Option<BusReply> {
        self.bus(request)
    }

    fn on_input(&mut self, name: &str) {
        if let Some((x, y)) = Self::read_input_point(name, "touch_down:") {
            self.set_point(x, y, 0);
            self.gesture = 0;
            return;
        }
        if let Some((x, y)) = Self::read_input_point(name, "touch_move:") {
            self.set_point(x, y, 2);
            self.gesture = 0;
            return;
        }
        if let Some((what, value)) = set_value(name) {
            let coordinate = value.clamp(0.0, 4095.0) as u16;
            match what {
                "x" => self.x = coordinate,
                "y" => self.y = coordinate,
                _ => {}
            }
            return;
        }
        match name {
            "touch_up" => {
                self.fingers = 0;
                self.event = 1;
                self.gesture = 5;
            }
            "swipe_up" => self.gesture = 1,
            "swipe_down" => self.gesture = 2,
            "swipe_left" => self.gesture = 3,
            "swipe_right" => self.gesture = 4,
            "double_tap" => self.gesture = 0x0B,
            "long_press" => self.gesture = 0x0C,
            "reset" => self.reset_controller(),
            _ => {}
        }
    }

    fn state(&self) -> Option<State> {
        Some(
            State::new()
                .set("address", "0x15")
                .set("chip_id", format!("0x{:02X}", CHIP_ID))
                .set("project_id", format!("0x{:02X}", PROJECT_ID))
                .set("firmware_version", format!("0x{:02X}", FIRMWARE_VERSION))
                .set("pressed", self.fingers != 0)
                .set("x", self.x)
                .set("y", self.y)
                .set("gesture", self.gesture)
                .set("sleeping", self.sleeping),
        )
    }
}

esp_part::export_part!(Cst820, buffer = 1024);

#[cfg(test)]
mod tests {
    use super::*;

    fn wired() -> Cst820 {
        let mut part = Cst820::default();
        part.configure(&PartConfig {
            pins: [
                (SCL.to_string(), 5),
                (SDA.to_string(), 4),
                (RESET.to_string(), 7),
            ]
            .into(),
            addr: 0,
        });
        part
    }

    fn request(addr: u32, write: &[u8], read_len: u32) -> BusRequest {
        BusRequest {
            correlation_id: 17,
            op: Some(bus_request::Op::I2c(I2cTransaction {
                bus: 0,
                addr,
                write: write.to_vec(),
                read_len,
                continues: false,
            })),
        }
    }

    fn transfer(part: &mut Cst820, write: &[u8], read_len: u32) -> BusReply {
        part.on_bus(&request(ADDRESS, write, read_len)).unwrap()
    }

    #[test]
    fn claims_the_pinned_i2c_bus_and_watches_the_shared_reset() {
        assert_eq!(
            wired().claims(),
            [
                Claim {
                    what: Some(claim::What::I2c(I2cClaim {
                        addr: 0x15,
                        scl_pin: Some(5),
                        sda_pin: Some(4),
                    }))
                },
                watches(1 << 7)
            ]
        );
    }

    #[test]
    fn returns_the_sdk_recorded_identity_in_one_burst() {
        let mut part = wired();
        let reply = transfer(&mut part, &[REG_CHIP_ID], 3);
        assert!(reply.ack);
        assert_eq!(reply.correlation_id, 17);
        assert_eq!(reply.data, [0xB7, 0x28, 0xC3]);
    }

    #[test]
    fn returns_the_six_byte_frame_with_twelve_bit_coordinates() {
        let mut part = wired();
        part.on_input("touch_down:335,285");
        let reply = transfer(&mut part, &[REG_FRAME], 6);
        assert!(reply.ack);
        assert_eq!(reply.data, [0, 1, 1, 0x4F, 1, 0x1D]);
    }

    #[test]
    fn touch_release_clears_the_finger_count_and_sets_up_event() {
        let mut part = wired();
        part.on_input("touch_down:80,215");
        part.on_input("touch_up");
        let reply = transfer(&mut part, &[REG_FRAME], 6);
        assert_eq!(reply.data[1], 0);
        assert_eq!(reply.data[2] >> 6, 1);
    }

    #[test]
    fn configuration_registers_round_trip_without_changing_identity() {
        let mut part = wired();
        assert!(transfer(&mut part, &[0xEC, 0x07], 0).ack);
        assert_eq!(transfer(&mut part, &[0xEC], 1).data, [0x07]);
        assert_eq!(transfer(&mut part, &[REG_CHIP_ID], 3).data, [0xB7, 0x28, 0xC3]);
    }

    #[test]
    fn deep_sleep_nacks_until_the_active_low_board_reset() {
        let mut part = wired();
        assert!(transfer(&mut part, &[REG_SLEEP, SLEEP_DEEP], 0).ack);
        assert!(!transfer(&mut part, &[REG_CHIP_ID], 3).ack);
        part.on_pins(0, 1_000); // GPIO7 low: display/touch reset asserted.
        assert!(transfer(&mut part, &[REG_CHIP_ID], 3).ack);
    }

    #[test]
    fn unknown_interactions_do_not_change_the_frame() {
        let mut part = wired();
        part.on_input("poke");
        assert_eq!(transfer(&mut part, &[REG_FRAME], 6).data, [0; 6]);
    }
}
