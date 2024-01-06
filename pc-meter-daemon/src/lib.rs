use hidapi::{HidDevice, HidError};
use sysinfo::{Components, Disks, System};

const USER_REPORT: u8 = 1;
const SYSTEM_REPORT: u8 = 0;

pub fn send_system_report(device: &HidDevice, sys: &System) -> Result<usize, HidError> {
    let mut buf = [0u8; 64];

    buf[0] = 0;
    buf[1] = SYSTEM_REPORT;
    buf[2] = sys.global_cpu_info().cpu_usage() as u8;
    buf[3] = (sys.used_memory() * 100 / sys.total_memory()) as u8;
    buf[4] = sys.cpus().len() as u8;
    // buf 5..9 remain empty for now
    for i in 0..sys.cpus().len() {
        if i + 10 == buf.len() {
            break;
        };
        buf[i + 10] = sys.cpus()[i].cpu_usage() as u8;
    }

    device.write(&buf)
}

pub fn send_user_report(
    device: &HidDevice,
    sys: &System,
    components: &Components,
    disks: &Disks,
) -> Result<usize, HidError> {
    let mut buf = [0u8; 64];
    let load_avg = System::load_average();

    buf[0] = 0;
    buf[1] = USER_REPORT;
    buf[2] = (sys.used_swap() * 100 / sys.total_swap()) as u8;
    buf[3] = load_avg.one.floor() as u8;
    buf[4] = load_avg.one.fract() as u8;
    buf[5] = load_avg.five.floor() as u8;
    buf[6] = load_avg.five.fract() as u8;
    buf[7] = load_avg.fifteen.floor() as u8;
    buf[8] = load_avg.fifteen.fract() as u8;
    buf[9] = disks.list().len() as u8;
    for i in 0..disks.list().len() {
        buf[i + 10] =
            (disks.list()[i].available_space() * 100 / disks.list()[i].total_space()) as u8;
        if i == 9 {
            break;
        }; // write only until buf[19]
    }
    for i in 0..components.list().len() {
        buf[i + 20] = components.list()[i].temperature() as u8;
        if i == 19 {
            break;
        }; // write only until buf[39]
    }
    device.write(&buf)
}
