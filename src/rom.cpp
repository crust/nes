#include "rom.h"

#include <fstream>
#include <unordered_map>

using std::ifstream;
using std::cout;
using std::setw;
using std::hex;
using std::vector;
using std::thread;
using std::string;
using std::exception;
using std::runtime_error;

static const unsigned VROM_Granularity = 0x400, VROM_Pages = 0x2000 / VROM_Granularity;
static const unsigned ROM_Granularity = 0x2000, ROM_Pages = 0x10000 / ROM_Granularity;

void ROM::write_prg(uint8_t value, uint16_t addr) {
  writef(*this, value, addr);
}

void ROM::write_chr(uint8_t value, uint16_t addr) {

}

uint8_t ROM::read_prg(uint16_t addr) const {
  return prg_bank[(addr / ROM_Granularity) % ROM_Pages][addr % ROM_Granularity];
}

uint8_t ROM::read_chr(uint16_t addr) const {
  return chr_bank[addr / 0x400][addr % 0x400];
}

void ROM::write_nt(uint8_t value, uint16_t addr) {
  uint16_t table = (addr >> 10) & 3;
  nametable[table][addr & 0x3ff] = value;
}

uint8_t ROM::read_nt(uint16_t addr) const {
  uint16_t table = (addr >> 10) & 3;
  return nametable[table][addr & 0x3ff];
}

uint8_t& ROM::getvbankref(uint16_t addr) {
  return chr_bank[addr / 0x400][addr % 0x400];
}

uint8_t const& ROM::getvbankref(uint16_t addr) const {
  return chr_bank[addr / 0x400][addr % 0x400];
}

static const std::unordered_map<uint8_t, std::function<void(ROM&, uint8_t, uint16_t)>> mapper {
  { 0, [](ROM& this_, uint8_t value, uint16_t addr){}},
};


uint8_t& ROM::getmemref(uint16_t addr) {
  return prg_bank[(addr / ROM_Granularity) % ROM_Pages][addr % ROM_Granularity];
}

template<unsigned npages, unsigned granu>
static void set_pages(
  std::vector<uint8_t*>& b,
  std::vector<uint8_t>& r,
  unsigned size,
  unsigned baseaddr,
  unsigned index
){
  auto rs = r.size();
  for(unsigned v = rs + index * size, p = baseaddr / granu;
    p < (baseaddr + size) / granu && p < npages;
    ++p, v += granu)
    b[p] = &r[v%rs];
}

static const auto& set_ROM = set_pages<ROM_Pages, ROM_Granularity>;
static const auto& set_VROM = set_pages<VROM_Pages, VROM_Granularity>;




ROM::ROM(std::string const& src):
  nt (0x800), 
  prg (0x2000),
  chr (0x2000),
  nametable {
    nt.data(),
    nt.data() + 0x400,
    nt.data(),
    nt.data() + 0x400,
  },
  prg_bank(8),
  chr_bank(8)
{

  ifstream file(src);

  if(!(
    file.get() == 'N' &&
    file.get() == 'E' &&
    file.get() == 'S' &&
    file.get() == 0x1A)){
    
    throw runtime_error ("Not iNES format");
    
  }
  
  int
    prg_rom_size { file.get() },
    chr_rom_size { file.get() },
    flag6 { file.get() },
    flag7 { file.get() },
    prg_ram_size { file.get() },
    flag9 { file.get() },
    flag10 { file.get() };

  file.seekg(0x10);
  
  int mapper_id { (flag6 >> 4) | (flag7 & 0xf0) };
  
  std::cout << "Mapper " << mapper_id << '\n';
  
  auto i = mapper.find(mapper_id);
  
  if(i == mapper.end())
    throw runtime_error ("Unsupported mapper");
  
  writef = i->second;

  if(prg_rom_size) {
    prg.resize(prg_rom_size * 0x4000);
  }

  if(chr_rom_size) {
    chr.resize(chr_rom_size * 0x2000);
  }

  file.read((char*)prg.data(), prg_rom_size * 0x4000);
  file.read((char*)chr.data(), chr_rom_size * 0x2000);

  set_VROM(chr_bank, chr, 0x2000, 0, 0);
  
  for(unsigned v = 0; v < 4; ++v)
    set_ROM(prg_bank, prg, 0x4000, v * 0x4000, v==3 ? -1 : 0);

}
