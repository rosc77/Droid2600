//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2016 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: CartCV.cxx 3316 2016-08-24 23:57:07Z stephena $
//============================================================================

#include "System.hxx"
#include "CartCV.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeCV::CartridgeCV(const uInt8* image, uInt32 size,
    const Settings& settings)
    : Cartridge(settings),
    mySize(size)
{
    if (mySize == 2048)
    {
      // Copy the ROM data into my buffer
        memcpy(myImage, image, 2048);
    }
    else if (mySize == 4096)
    {
      // The game has something saved in the RAM
      // Useful for MagiCard program listings

      // Copy the ROM data into my buffer
        memcpy(myImage, image + 2048, 2048);

        // Copy the RAM image into a buffer for use in reset()
        myInitialRAM = make_ptr<uInt8[]>(1024);
        memcpy(myInitialRAM.get(), image, 1024);
    }
    createCodeAccessBase(2048 + 1024);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeCV::reset()
{
    if (myInitialRAM)
    {
      // Copy the RAM image into my buffer
        memcpy(myRAM, myInitialRAM.get(), 1024);
    }
    else
        initializeRAM(myRAM, 1024);

    myBankChanged = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeCV::install(System& system)
{
    mySystem = &system;

    System::PageAccess access(this, System::PA_READ);

    // Map ROM image into the system
    for (uInt32 address = 0x1800; address < 0x2000;
        address += (1 << System::S_PAGE_SHIFT))
    {
        access.directPeekBase = &myImage[address & 0x07FF];
        access.codeAccessBase = &myCodeAccessBase[address & 0x07FF];
        mySystem->setPageAccess(address >> System::S_PAGE_SHIFT, access);
    }

    // Set the page accessing method for the RAM writing pages
    access.directPeekBase = 0;
    access.codeAccessBase = 0;
    access.type = System::PA_WRITE;
    for (uInt32 j = 0x1400; j < 0x1800; j += (1 << System::S_PAGE_SHIFT))
    {
        access.directPokeBase = &myRAM[j & 0x03FF];
        mySystem->setPageAccess(j >> System::S_PAGE_SHIFT, access);
    }

    // Set the page accessing method for the RAM reading pages
    access.directPokeBase = 0;
    access.type = System::PA_READ;
    for (uInt32 k = 0x1000; k < 0x1400; k += (1 << System::S_PAGE_SHIFT))
    {
        access.directPeekBase = &myRAM[k & 0x03FF];
        access.codeAccessBase = &myCodeAccessBase[2048 + (k & 0x03FF)];
        mySystem->setPageAccess(k >> System::S_PAGE_SHIFT, access);
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 CartridgeCV::peek(uInt16 address)
{
    if ((address & 0x0FFF) < 0x0800)  // Write port is at 0xF400 - 0xF800 (1024 bytes)
    {                                // Read port is handled in ::install()
      // Reading from the write port triggers an unwanted write
        uInt8 value = mySystem->getDataBusState(0xFF);

        if (bankLocked())
            return value;
        else
        {
            triggerReadFromWritePort(address);
            return myRAM[address & 0x03FF] = value;
        }
    }
    else
        return myImage[address & 0x07FF];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CartridgeCV::poke(uInt16, uInt8)
{
  // NOTE: This does not handle accessing RAM, however, this function 
  // should never be called for RAM because of the way page accessing 
  // has been setup
    return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CartridgeCV::patch(uInt16 address, uInt8 value)
{
    address &= 0x0FFF;

    if (address < 0x0800)
    {
      // Normally, a write to the read port won't do anything
      // However, the patch command is special in that ignores such
      // cart restrictions
      // The following will work for both reads and writes
        myRAM[address & 0x03FF] = value;
    }
    else
        myImage[address & 0x07FF] = value;

    return myBankChanged = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const uInt8* CartridgeCV::getImage(int& size) const
{
    size = 2048;
    return myImage;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CartridgeCV::save(Serializer& out) const
{
    try
    {
        out.putString(name());
        out.putByteArray(myRAM, 1024);
    }
    catch (...)
    {
        cerr << "ERROR: CartridgeCV::save" << endl;
        return false;
    }

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CartridgeCV::load(Serializer& in)
{
    try
    {
        if (in.getString() != name())
            return false;

        in.getByteArray(myRAM, 1024);
    }
    catch (...)
    {
        cerr << "ERROR: CartridgeCV::load" << endl;
        return false;
    }

    return true;
}
