//! Virtual TCA6408A GPIO expander used on the Kode Dot DVT.
//!
//! The register map follows the TCA6408A register pointer:
//! 0=input, 1=output, 2=polarity inversion, 3=configuration.
//! This first model covers the eight GPIOs, input-change interrupt,
//! output/configuration registers and the Kode Dot's documented pin use.
//! Electrical timing and analog behaviour are intentionally not modelled.

use esp_part::prelude::*;

const SCL: &str = "SCL";
const SDA: &str = "SDA";
const INT: &str = "INT";
const ADDRESS: u32 = 0x20;

const REG_INPUT: u8 = 0;
const REG_OUTPUT: u8 = 1;
const REG_POLARITY: u8 = 2;
const REG_CONFIG: u8 = 3;

const INPUT_MASK: u8 = 0x7F; // P0..P6 inputs; P7 is display VCI_EN output.

#[derive(Debug)]
struct Tca6408a {
    scl: Option<u32>,
    sda: Option<u32>,
    int_pin: Option<u32>,
    pointer: u8,
    input: u8,
    output: u8,
    polarity: u8,
    config: u8,
    previous_inputs: u8,
    irq: bool,
    irq_reported: Option<bool>,
}

impl Default for Tca6408a {
    fn default() -> Self {
        Self {
            scl: None,
            sda: None,
            int_pin: None,
            pointer: REG_INPUT,
            input: 0xFF,
            output: 0xFF,
            polarity: 0,
            config: 0xFF,
            previous_inputs: 0xFF,
            irq: false,
            irq_reported: None,
        }
    }
}

impl Tca6408a {
    fn pin_values(&self) -> u8 {
        (self.input & self.config) | (self.output & !self.config)
    }

    fn read_register(&mut self, register: u8) -> u8 {
        match register & 0x03 {
            REG_INPUT => {
                let value = self.pin_values() ^ self.polarity;
                self.previous_inputs = self.input & self.config;
                self.irq = false;
                value
            }
            REG_OUTPUT => self.output,
            REG_POLARITY => self.polarity,
            REG_CONFIG => self.config,
            _ => 0xFF,
        }
    }

    fn write_register(&mut self, register: u8, value: u8) {
        match register & 0x03 {
            REG_OUTPUT => self.output = value,
            REG_POLARITY => self.polarity = value,
            REG_CONFIG => self.config = value,
            _ => {} // Input port is read-only.
        }
    }

    fn set_input(&mut self, bit: u8, high: bool) {
        let mask = 1u8 << bit;
        if high {
            self.input |= mask;
        } else {
            self.input &= !mask;
        }
        let changed_inputs = (self.input ^ self.previous_inputs) & self.config;
        if changed_inputs != 0 {
            self.irq = true;
        }
    }

    fn interact(&mut self, name: &str) {
        let action = match name {
            "bottom_press" => Some((0, false)),
            "bottom_release" => Some((0, true)),
            "pad_up_press" => Some((1, false)),
            "pad_up_release" => Some((1, true)),
            "pad_left_press" => Some((2, false)),
            "pad_left_release" => Some((2, true)),
            "pad_down_press" => Some((3, false)),
            "pad_down_release" => Some((3, true)),
            "pad_right_press" => Some((4, false)),
            "pad_right_release" => Some((4, true)),
            "imu_irq_assert" => Some((5, false)),
            "imu_irq_release" => Some((5, true)),
            "touch_irq_assert" => Some((6, false)),
            "touch_irq_release" => Some((6, true)),
            _ => None,
        };
        if let Some((bit, high)) = action {
            self.set_input(bit, high);
        }
    }

    fn bus(&mut self, request: &BusRequest) -> Option<BusReply> {
        let i2c = match request.op.as_ref()? {
            bus_request::Op::I2c(i2c) => i2c,
            _ => return None,
        };
        let mut reply = BusReply {
            correlation_id: request.correlation_id,
            ack: true,
            op: BusOp::I2c as i32,
            ..Default::default()
        };
        if i2c.addr != ADDRESS {
            reply.ack = false;
            return Some(reply);
        }

        if !i2c.write.is_empty() {
            self.pointer = i2c.write[0] & 0x03;
            for value in i2c.write.iter().skip(1) {
                self.write_register(self.pointer, *value);
                self.pointer = self.pointer.wrapping_add(1) & 0x03;
            }
        }

        if i2c.read_len > 0 {
            let mut bytes = Vec::with_capacity(i2c.read_len as usize);
            for _ in 0..i2c.read_len {
                bytes.push(self.read_register(self.pointer));
                self.pointer = self.pointer.wrapping_add(1) & 0x03;
            }
            reply.data = bytes;
        }
        Some(reply)
    }
}

impl Device for Tca6408a {
    const NAME: &'static str = "kode_dot_tca6408a";
    const ON_BUS: bool = true;

    fn configure(&mut self, config: &PartConfig) {
        self.scl = config.pins.get(SCL).copied();
        self.sda = config.pins.get(SDA).copied();
        self.int_pin = config.pins.get(INT).copied();
    }

    fn claims(&self) -> Vec<Claim> {
        let i2c = I2cClaim {
            addr: ADDRESS,
            scl_pin: self.scl,
            sda_pin: self.sda,
        };
        let mut claims = vec![Claim { what: Some(claim::What::I2c(i2c)) }];
        if let Some(pin) = self.int_pin {
            claims.push(drives(1u64 << pin));
        }
        claims
    }

    fn on_bus(&mut self, request: &BusRequest) -> Option<BusReply> {
        self.bus(request)
    }

    fn on_input(&mut self, name: &str) {
        self.interact(name);
    }

    fn poll(&mut self, now_ns: u64) -> Option<EmulatorMessage> {
        let pin = self.int_pin?;
        if self.irq_reported == Some(self.irq) {
            return None;
        }
        self.irq_reported = Some(self.irq);
        // TCA6408A INT is active low and open-drain. High releases the line.
        Some(drive(pin, !self.irq, now_ns))
    }

    fn state(&self) -> Option<State> {
        Some(
            State::new()
                .set("address", "0x20")
                .set("input", format!("0x{:02X}", self.input))
                .set("output", format!("0x{:02X}", self.output))
                .set("polarity", format!("0x{:02X}", self.polarity))
                .set("configuration", format!("0x{:02X}", self.config))
                .set("display_power_enable", (self.output & 0x80) != 0)
                .set("interrupt_asserted", self.irq),
        )
    }
}

esp_part::export_part!(Tca6408a, buffer = 512);

#[cfg(test)]
mod tests {
    use super::*;
    use esp_part::Payload;

    fn configured() -> Tca6408a {
        let mut device = Tca6408a::default();
        device.configure(&PartConfig {
            pins: [
                (SCL.to_string(), 5),
                (SDA.to_string(), 4),
                (INT.to_string(), 6),
            ].into(),
            ..Default::default()
        });
        device
    }

    fn transaction(write: &[u8], read_len: u32) -> BusRequest {
        BusRequest {
            correlation_id: 42,
            op: Some(bus_request::Op::I2c(I2cTransaction {
                bus: 0,
                addr: ADDRESS,
                write: write.to_vec(),
                read_len,
                continues: false,
            })),
        }
    }

    #[test]
    fn claims_the_official_i2c_address_and_interrupt_pin() {
        let claims = configured().claims();
        assert_eq!(claims.len(), 2);
    }

    #[test]
    fn reads_power_on_configuration_and_input_registers() {
        let mut d = configured();
        let input = d.bus(&transaction(&[REG_INPUT], 1)).unwrap();
        assert_eq!(input.data, vec![0xFF]);
        let cfg = d.bus(&transaction(&[REG_CONFIG], 1)).unwrap();
        assert_eq!(cfg.data, vec![0xFF]);
    }

    #[test]
    fn writes_configuration_and_output_registers() {
        let mut d = configured();
        d.bus(&transaction(&[REG_CONFIG, INPUT_MASK], 0)).unwrap();
        d.bus(&transaction(&[REG_OUTPUT, 0x00], 0)).unwrap();
        assert_eq!(d.config, INPUT_MASK);
        assert_eq!(d.output, 0x00);
    }

    #[test]
    fn button_changes_assert_interrupt_until_input_port_is_read() {
        let mut d = configured();
        d.interact("bottom_press");
        assert!(d.irq);
        let msg = d.poll(1_000).expect("IRQ should be driven low");
        assert!(matches!(msg.payload, Some(Payload::GpioDrive(_))));
        let _ = d.bus(&transaction(&[REG_INPUT], 1)).unwrap();
        assert!(!d.irq);
        assert!(d.poll(2_000).is_some(), "IRQ should return high after read");
    }

    #[test]
    fn unrecognised_interactions_do_not_change_inputs() {
        let mut d = configured();
        d.interact("click");
        assert_eq!(d.input, 0xFF);
        assert!(!d.irq);
    }
}
