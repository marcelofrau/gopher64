use crate::device;
use crate::retroachievements;
use crate::ui;
use std::ops::{Deref, DerefMut};

//const RDRAM_CONFIG_REG: usize = 0;
//const RDRAM_DEVICE_ID_REG: usize = 1;
//const RDRAM_DELAY_REG: usize = 2;
const RDRAM_MODE_REG: usize = 3;
//const RDRAM_REF_INTERVAL_REG: usize = 4;
//const RDRAM_REF_ROW_REG: usize = 5;
//const RDRAM_RAS_INTERVAL_REG: usize = 6;
//const RDRAM_MIN_INTERVAL_REG: usize = 7;
//const RDRAM_ADDR_SELECT_REG: usize = 8;
//const RDRAM_DEVICE_MANUF_REG: usize = 9;
pub const RDRAM_REGS_COUNT: usize = 10;

pub const RDRAM_MASK: usize = 0xFFFFFF;

const RDRAM_ALIGNMENT: usize = 64 * 1024;

// UWP-PORT NOTE (upstream-PR candidate, not a cfg hook): AlignedRdram replaces an
// alloc_zeroed + Vec::from_raw_parts pattern that handed a 64 KiB-aligned pointer to a Vec<u8>
// whose deallocation layout is byte-aligned — undefined behavior that caused a symbolized heap
// crash on Xbox. It keeps an ordinary Vec and exposes a 64 KiB-aligned slice inside it. The
// postcard savestate encoding is preserved (see the round-trip test below). This applies to all
// platforms and should be proposed upstream by hand; tracked under "Fidelity deviations requiring
// ongoing review" in docs/xbox-port/00-upstream-fidelity-rules.md.
#[derive(Default)]
pub struct AlignedRdram {
    storage: Vec<u8>,
    offset: usize,
    len: usize,
}

impl AlignedRdram {
    fn zeroed(len: usize) -> Self {
        if len == 0 {
            return Self::default();
        }
        // Keep the original Vec allocation so it is freed with its original layout.
        let storage = vec![
            0;
            len.checked_add(RDRAM_ALIGNMENT - 1)
                .expect("RDRAM size overflow")
        ];
        let offset = storage.as_ptr().align_offset(RDRAM_ALIGNMENT);
        Self {
            storage,
            offset,
            len,
        }
    }
}

impl Deref for AlignedRdram {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        &self.storage[self.offset..self.offset + self.len]
    }
}

impl DerefMut for AlignedRdram {
    fn deref_mut(&mut self) -> &mut [u8] {
        &mut self.storage[self.offset..self.offset + self.len]
    }
}

impl Clone for AlignedRdram {
    fn clone(&self) -> Self {
        let mut result = Self::zeroed(self.len);
        result.copy_from_slice(self);
        result
    }

    fn clone_from(&mut self, source: &Self) {
        if self.len != source.len {
            *self = Self::zeroed(source.len);
        }
        self.copy_from_slice(source);
    }
}

impl serde::Serialize for AlignedRdram {
    fn serialize<S: serde::Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serde::Serialize::serialize(&**self, serializer)
    }
}

impl<'de> serde::Deserialize<'de> for AlignedRdram {
    fn deserialize<D: serde::Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        let bytes = <Vec<u8> as serde::Deserialize>::deserialize(deserializer)?;
        let mut result = Self::zeroed(bytes.len());
        result.copy_from_slice(&bytes);
        Ok(result)
    }
}

#[cfg(test)]
mod aligned_rdram_tests {
    use super::*;

    #[test]
    fn allocation_clone_and_reuse_preserve_alignment_and_contents() {
        for size in [0x400000, 0x800000] {
            let mut original = AlignedRdram::zeroed(size);
            assert_eq!(original.as_ptr() as usize % RDRAM_ALIGNMENT, 0);
            assert!(original.iter().all(|byte| *byte == 0));
            original[0] = 0x12;
            original[size - 1] = 0x34;
            let mut clone = original.clone();
            assert_eq!(clone.as_ptr() as usize % RDRAM_ALIGNMENT, 0);
            assert_eq!(&*clone, &*original);
            let address = clone.as_ptr();
            original[0] = 0x56;
            clone.clone_from(&original);
            assert_eq!(clone.as_ptr(), address);
            assert_eq!(&*clone, &*original);
        }
    }

    #[test]
    fn previous_savestate_encoding_round_trips() {
        #[derive(serde::Serialize)]
        struct PreviousRdram {
            mem: Vec<u8>,
            size: u32,
            regs: [[u32; RDRAM_REGS_COUNT]; 4],
        }
        let previous = PreviousRdram {
            mem: (0..4096).map(|x| x as u8).collect(),
            size: 4096,
            regs: [[42; RDRAM_REGS_COUNT]; 4],
        };
        let bytes = postcard::to_stdvec(&previous).unwrap();
        let decoded: Rdram = postcard::from_bytes(&bytes).unwrap();
        assert_eq!(&*decoded.mem, previous.mem);
        assert_eq!(decoded.mem.as_ptr() as usize % RDRAM_ALIGNMENT, 0);
        assert_eq!(postcard::to_stdvec(&decoded).unwrap(), bytes);
    }
}

#[derive(Clone, serde::Serialize, serde::Deserialize)]
pub struct Rdram {
    pub mem: AlignedRdram,
    pub size: u32,
    pub regs: [[u32; RDRAM_REGS_COUNT]; 4],
}

pub fn read_mem_fast(
    device: &device::Device,
    address: u64,
    _access_size: device::memory::AccessSize,
) -> u32 {
    let masked_address = address as usize & RDRAM_MASK;
    u32::from_ne_bytes(
        device
            .rdram
            .mem
            .get(masked_address..masked_address + 4)
            .unwrap_or(&[0; 4])
            .try_into()
            .unwrap_or_default(),
    )
}

pub fn read_mem(
    device: &mut device::Device,
    address: u64,
    access_size: device::memory::AccessSize,
) -> u32 {
    device::cop0::add_cycles(
        device,
        match access_size {
            device::memory::AccessSize::Word => const { rdram_cycles_per_word(4) },
            device::memory::AccessSize::Dword => const { rdram_cycles_per_word(8) },
            device::memory::AccessSize::Dcache => const { rdram_cycles_per_word(16) },
            device::memory::AccessSize::Icache => const { rdram_cycles_per_word(32) },
            device::memory::AccessSize::None => unreachable!(),
        },
    );
    let masked_address = address as usize & RDRAM_MASK;

    ui::video::check_framebuffers(masked_address as u32, 4);

    u32::from_ne_bytes(
        device
            .rdram
            .mem
            .get(masked_address..masked_address + 4)
            .unwrap_or(&[0; 4])
            .try_into()
            .unwrap_or_default(),
    )
}

pub fn write_mem(device: &mut device::Device, address: u64, value: u32, mask: u32) {
    ui::video::check_framebuffers(address as u32, 4);

    let mut data = u32::from_ne_bytes(
        device
            .rdram
            .mem
            .get(address as usize..(address + 4) as usize)
            .unwrap_or(&[0; 4])
            .try_into()
            .unwrap_or_default(),
    );
    device::memory::masked_write_32(&mut data, value, mask);
    device
        .rdram
        .mem
        .get_mut(address as usize..(address + 4) as usize)
        .unwrap_or(&mut [0; 4])
        .copy_from_slice(&data.to_ne_bytes());
}

pub fn write_mem_repeat(device: &mut device::Device, address: u64, value: u32, mask: u32) {
    if mask != 0xFFFFFFFF {
        panic!("RDRAM write_mem_repeat called with mask {:#x}", mask);
    }

    let repeat_length =
        (device.mi.regs[device::mi::MI_INIT_MODE_REG] & device::mi::MI_INIT_LENGTH_MASK) + 1;

    if !repeat_length.is_multiple_of(4) {
        panic!(
            "RDRAM write_mem_repeat called with non-word-aligned length {}",
            repeat_length
        );
    }

    ui::video::check_framebuffers(address as u32, repeat_length);

    for i in (0..repeat_length as u64).step_by(4) {
        device
            .rdram
            .mem
            .get_mut((address + i) as usize..(address + i) as usize + 4)
            .unwrap_or(&mut [0; 4])
            .copy_from_slice(&value.to_ne_bytes());
    }

    device.mi.regs[device::mi::MI_INIT_MODE_REG] &= !device::mi::MI_INIT_MODE;
    for i in 0..(0x3F00000 >> 16) {
        device.memory.memory_map_write[i] = device::rdram::write_mem;
    }
}

pub fn read_regs(
    device: &mut device::Device,
    address: u64,
    _access_size: device::memory::AccessSize,
) -> u32 {
    device::cop0::add_cycles(device, 20);
    let chip_id = (address >> 13) & 3;
    let reg = (address & 0x3FF) >> 2;
    match reg as usize {
        RDRAM_MODE_REG => device.pi.regs[reg as usize] ^ 0xc0c0c0c0,
        0x80 => 0x00000000, //Row, needed for libdragon
        _ => device.rdram.regs[chip_id as usize][reg as usize],
    }
}

pub fn write_regs(device: &mut device::Device, address: u64, value: u32, mask: u32) {
    let chip_id = (address >> 13) & 3;
    let reg = (address & 0x3FF) >> 2;
    device::memory::masked_write_32(
        &mut device.rdram.regs[chip_id as usize][reg as usize],
        value,
        mask,
    )
}

pub fn init(device: &mut device::Device) {
    device.rdram.mem = AlignedRdram::zeroed(device.rdram.size as usize);

    retroachievements::set_rdram(device.rdram.mem.as_ptr(), device.rdram.size as usize);

    // hack, skip RDRAM initialization
    device
        .rdram
        .mem
        .get_mut(0x318..0x318 + 4)
        .unwrap_or(&mut [0; 4])
        .copy_from_slice(&device.rdram.size.to_ne_bytes());
    // hack, skip RDRAM initialization
    device
        .rdram
        .mem
        .get_mut(0x3f0..0x3f0 + 4)
        .unwrap_or(&mut [0; 4])
        .copy_from_slice(&device.rdram.size.to_ne_bytes());

    device.ri.regs[device::ri::RI_MODE_REG] = 0x0e;
    device.ri.regs[device::ri::RI_CONFIG_REG] = 0x40;
}

pub const fn rdram_calculate_cycles(length: u64) -> u64 {
    31 + (length / 3) // https://hcs64.com/dma.html, https://github.com/rasky/n64-systembench
}

const fn rdram_cycles_per_word(access_size: u64) -> u64 {
    rdram_calculate_cycles(access_size) / (access_size / 4)
}
