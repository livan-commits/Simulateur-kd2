use esp_part::prelude::*;

const SCL: &str = "SCL";
const SDA: &str = "SDA";
const TCA_ADDR: u32 = 0x20;
const CST_ADDR: u32 = 0x15;
const IRQ_PIN: u32 = 6;
const PRESS_NS: u64 = 60_000_000;

struct KodeBoard {
    scl: Option<u32>,
    sda: Option<u32>,
    tca_input: u8,
    tca_output: u8,
    tca_polarity: u8,
    tca_config: u8,
    tca_ptr: u8,
    cst_regs: [u8; 256],
    cst_ptr: u8,
    touch_pressed: bool,
    touch_x: u16,
    touch_y: u16,
    event_active: bool,
    release_at_ns: Option<u64>,
    irq_high: bool,
    last_irq_high: Option<bool>,
}

impl Default for KodeBoard {
    fn default() -> Self {
        let mut cst_regs = [0u8; 256];
        // CST820 identity block: the SDK accepts the CST8xx family nibble 0xB.
        cst_regs[0xA7] = 0xB7;
        cst_regs[0xA8] = 0x01;
        cst_regs[0xA9] = 0x01;
        Self {
            scl: None,
            sda: None,
            tca_input: 0xFF,
            tca_output: 0xFF,
            tca_polarity: 0x00,
            tca_config: 0xFF,
            tca_ptr: 0,
            cst_regs,
            cst_ptr: 0,
            touch_pressed: false,
            touch_x: 0,
            touch_y: 0,
            event_active: false,
            release_at_ns: None,
            irq_high: true,
            last_irq_high: None,
        }
    }
}

impl Device for KodeBoard {
    const NAME: &'static str = "kode-dot-board";
    const ON_BUS: bool = true;

    fn configure(&mut self, config: &PartConfig) {
        self.scl = config.pins.get(SCL).copied();
        self.sda = config.pins.get(SDA).copied();
    }

    fn claims(&self) -> Vec<Claim> {
        let tca = I2cClaim { addr: TCA_ADDR, scl_pin: self.scl, sda_pin: self.sda };
        let cst = I2cClaim { addr: CST_ADDR, scl_pin: self.scl, sda_pin: self.sda };
        vec![
            Claim { what: Some(claim::What::I2c(tca)) },
            Claim { what: Some(claim::What::I2c(cst)) },
            drives(1u64 << IRQ_PIN),
        ]
    }

    fn on_bus(&mut self, request: &BusRequest) -> Option<BusReply> {
        let mut reply = BusReply {
            correlation_id: request.correlation_id,
            op: BusOp::I2c as i32,
            ack: false,
            ..Default::default()
        };
        let Some(bus_request::Op::I2c(i2c)) = &request.op else {
            return Some(reply);
        };
        match i2c.addr {
            TCA_ADDR => {
                reply.ack = true;
                self.tca_transaction(i2c, &mut reply.data);
            }
            CST_ADDR => {
                reply.ack = true;
                self.cst_transaction(i2c, &mut reply.data);
            }
            _ => {}
        }
        Some(reply)
    }

    fn on_input(&mut self, name: &str) {
        if let Some(coords) = name.strip_prefix("tap:") {
            if let Some((x, y)) = coords.split_once(',') {
                if let (Ok(x), Ok(y)) = (x.parse::<u16>(), y.parse::<u16>()) {
                    self.touch_x = x.min(4095);
                    self.touch_y = y.min(4095);
                    self.touch_pressed = true;
                    self.set_touch_frame();
                    self.tca_input &= !(1 << 6);
                    self.start_interrupt();
                }
            }
            return;
        }

        let bit = match name {
            "button:bot" => Some(0),
            "button:top" => Some(1),
            "button:left" => Some(2),
            "button:down" => Some(3),
            "button:right" => Some(4),
            _ => None,
        };
        if let Some(bit) = bit {
            self.tca_input &= !(1 << bit);
            self.start_interrupt();
        }
    }

    fn poll(&mut self, now_ns: u64) -> Option<EmulatorMessage> {
        if self.event_active && self.release_at_ns.is_none() {
            self.release_at_ns = Some(now_ns.saturating_add(PRESS_NS));
            self.irq_high = false;
        }
        if let Some(deadline) = self.release_at_ns {
            if now_ns >= deadline {
                self.touch_pressed = false;
                self.set_touch_frame();
                self.tca_input = 0xFF;
                self.event_active = false;
                self.release_at_ns = None;
                self.irq_high = true;
            }
        }
        if self.last_irq_high != Some(self.irq_high) {
            self.last_irq_high = Some(self.irq_high);
            return Some(drive(IRQ_PIN, self.irq_high, now_ns));
        }
        None
    }

    fn state(&self) -> Option<State> {
        Some(
            State::new()
                .set("tca6408a", "0x20")
                .set("cst820", "0x15")
                .set("touch", self.touch_pressed)
                .set("x", self.touch_x)
                .set("y", self.touch_y)
                .set("buttons", format!("0x{:02X}", self.tca_input & 0x1F))
                .set("panel_power", (self.tca_output & (1 << 7)) != 0),
        )
    }
}

impl KodeBoard {
    fn start_interrupt(&mut self) {
        self.event_active = true;
        self.release_at_ns = None;
    }

    fn set_touch_frame(&mut self) {
        let x = self.touch_x;
        let y = self.touch_y;
        self.cst_regs[0x01] = 0;
        self.cst_regs[0x02] = if self.touch_pressed { 1 } else { 0 };
        self.cst_regs[0x03] = ((x >> 8) as u8) & 0x0F;
        self.cst_regs[0x04] = x as u8;
        self.cst_regs[0x05] = ((y >> 8) as u8) & 0x0F;
        self.cst_regs[0x06] = y as u8;
    }

    fn tca_read(&self, reg: u8) -> u8 {
        match reg {
            0x00 => self.tca_input,
            0x01 => self.tca_output,
            0x02 => self.tca_polarity,
            0x03 => self.tca_config,
            _ => 0xFF,
        }
    }

    fn tca_write(&mut self, reg: u8, value: u8) {
        match reg {
            0x01 => self.tca_output = value,
            0x02 => self.tca_polarity = value,
            0x03 => self.tca_config = value,
            _ => {}
        }
    }

    fn tca_transaction(&mut self, i2c: &I2cTransaction, data: &mut Vec<u8>) {
        if !i2c.continues && !i2c.write.is_empty() {
            self.tca_ptr = i2c.write[0];
            if i2c.write.len() >= 2 {
                self.tca_write(self.tca_ptr, i2c.write[1]);
                self.tca_ptr = self.tca_ptr.wrapping_add(1);
            }
        }
        for _ in 0..i2c.read_len {
            data.push(self.tca_read(self.tca_ptr));
            self.tca_ptr = self.tca_ptr.wrapping_add(1);
        }
    }

    fn cst_transaction(&mut self, i2c: &I2cTransaction, data: &mut Vec<u8>) {
        if !i2c.continues && !i2c.write.is_empty() {
            self.cst_ptr = i2c.write[0];
            for value in i2c.write.iter().skip(1) {
                self.cst_regs[self.cst_ptr as usize] = *value;
                self.cst_ptr = self.cst_ptr.wrapping_add(1);
            }
        }
        for _ in 0..i2c.read_len {
            data.push(self.cst_regs[self.cst_ptr as usize]);
            self.cst_ptr = self.cst_ptr.wrapping_add(1);
        }
    }
}

esp_part::export_part!(KodeBoard, buffer = 512);

#[cfg(test)]
mod tests {
    use super::*;

    fn i2c_request(addr: u32, write: &[u8], read_len: u32) -> BusRequest {
        BusRequest {
            correlation_id: 7,
            op: Some(bus_request::Op::I2c(I2cTransaction {
                addr,
                write: write.to_vec(),
                read_len,
                continues: false,
                ..Default::default()
            })),
            ..Default::default()
        }
    }

    #[test]
    fn cst820_reports_the_expected_family_id() {
        let mut board = KodeBoard::default();
        let reply = board.on_bus(&i2c_request(CST_ADDR, &[0xA7], 3)).unwrap();
        assert!(reply.ack);
        assert_eq!(reply.data, [0xB7, 0x01, 0x01]);
    }

    #[test]
    fn tca6408a_registers_are_readable_and_writable() {
        let mut board = KodeBoard::default();
        let read = board.on_bus(&i2c_request(TCA_ADDR, &[0x03], 1)).unwrap();
        assert!(read.ack);
        assert_eq!(read.data, [0xFF]);

        let write = board.on_bus(&i2c_request(TCA_ADDR, &[0x01, 0x7F], 0)).unwrap();
        assert!(write.ack);
        let read = board.on_bus(&i2c_request(TCA_ADDR, &[0x01], 1)).unwrap();
        assert_eq!(read.data, [0x7F]);
    }

    #[test]
    fn browser_tap_becomes_a_cst820_frame_and_expander_irq() {
        let mut board = KodeBoard::default();
        board.on_input("tap:123,456");
        assert_eq!(board.tca_input & (1 << 6), 0);
        assert!(board.poll(0).is_some());

        let frame = board.on_bus(&i2c_request(CST_ADDR, &[0x01], 6)).unwrap();
        assert_eq!(frame.data, [0, 1, 0, 123, 1, 200]);

        assert!(board.poll(PRESS_NS + 1).is_some());
        assert_eq!(board.tca_input, 0xFF);
    }
}
