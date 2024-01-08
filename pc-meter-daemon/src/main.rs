use clap::Parser;
use hidapi::HidApi;
use pc_meterd::{send_system_report, send_user_report};
use std::{thread, time};
use sysinfo::{Components, Disks, System};

const VID: u16 = 0x2e8a;
const PID: u16 = 0xc011;

#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Args {
    /// Length of pause between each time the data is sent to the PC-Meter (ms)
    #[arg(short = 'i', long, default_value_t = 1000)]
    pub interval: u16,
    #[arg(short = 's', long, default_value_t = false)]
    /// Send the system report (conflicts with kernel module)
    pub system: bool,
    #[arg(short = 'c', long, default_value_t = false)]
    /// Print the components list with buffer positions and exit
    pub components: bool,
    #[arg(short = 'd', long, default_value_t = false)]
    /// Print the disks list with buffer positions and exit
    pub disks: bool,
}

fn main() {
    let args = Args::parse();
    let interval = time::Duration::from_millis(args.interval.into());
    let api = HidApi::new().expect("Failed to create API instance");

    let mut sys = System::new_all();
    let mut comp = Components::new_with_refreshed_list();
    let mut disks = Disks::new_with_refreshed_list();

    if args.components {
        let mut i = 20;
        for component in comp.iter() {
            println!("buf[{i}]: {component:?}");
            i += 1;
        }
        return;
    }
    if args.disks {
        let mut i = 10;
        for disk in disks.iter() {
            println!("buf[{i}]: {disk:?}");
            i += 1;
        }
        return;
    }

    loop {
        let pcmeter = api.open(VID, PID);
        match pcmeter {
            Ok(device) => loop {
                sys.refresh_all();
                comp.refresh();
                disks.refresh();

                if args.system {
                    if let Err(e) = send_system_report(&device, &sys) {
                        eprintln!("Write error: {}, device disconnected?", e);
                        break;
                    }
                }
                if let Err(e) = send_user_report(&device, &sys, &comp, &disks) {
                    eprintln!("Write error: {}, device disconnected?", e);
                    break;
                }
                thread::sleep(interval);
            },
            Err(e) => {
                eprintln!("Failed to open device: {}", e);
                thread::sleep(interval * 10);
            }
        }
    }
}
