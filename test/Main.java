import pico.hardware.SystemTimer;
import pico.hardware.Flash;
import java.io.IOException;

class Main {
    public static void main(String[] args) {
        SystemTimer.delayMs(5000);
        System.out.println("=== Flash API test ===");

        int pass = 0;
        int fail = 0;

        // Use a safe flash offset well past firmware, jar, config, and LittleFS
        // On 2MB Pico: LFS ends at 0x200000, so use 0x1F0000 (last 64KB before end)
        // Actually LFS starts at 0x190000, so we need to use space BEFORE LFS
        // Config is at 0x180000 (4KB). Use 0x184000 - one sector after config.
        // Wait - that's inside LFS region. Let's use the very last sector of flash
        // which is at 0x1FF000 (inside LFS region but we can test raw flash there).
        // Better: use a dedicated test sector at 0x170000 (between jar end and config)
        int testOffset = 0x170000;
        int sectorSize = Flash.SECTOR_SIZE; // 4096
        int pageSize = Flash.PAGE_SIZE;     // 256

        // 1. Read original content (to restore later)
        byte[] original = new byte[sectorSize];
        Flash.read(testOffset, original, 0, sectorSize);
        System.out.println("Read original sector OK");

        // 2. Erase the sector
        try {
            Flash.erase(testOffset, sectorSize);
            System.out.println("Erase OK");
        } catch (IOException e) {
            System.out.println("Erase FAIL: " + e.getMessage());
            fail++;
        }

        // 3. Verify erased (should be all 0xFF)
        byte[] buf = new byte[sectorSize];
        Flash.read(testOffset, buf, 0, sectorSize);
        boolean allFF = true;
        for (int i = 0; i < sectorSize; i++) {
            if ((buf[i] & 0xFF) != 0xFF) {
                allFF = false;
                System.out.println("Erase verify FAIL at offset " + i + ": got 0x" + Integer.toHexString(buf[i] & 0xFF));
                break;
            }
        }
        if (allFF) {
            System.out.println("Erase verify: PASS (all 0xFF)");
            pass++;
        } else {
            fail++;
        }

        // 4. Program a page (256 bytes) with a pattern
        byte[] page = new byte[pageSize];
        for (int i = 0; i < pageSize; i++) {
            page[i] = (byte)(i & 0xFF);
        }
        try {
            Flash.program(testOffset, page, 0, pageSize);
            System.out.println("Program OK");
        } catch (IOException e) {
            System.out.println("Program FAIL: " + e.getMessage());
            fail++;
        }

        // 5. Read back and verify
        byte[] readback = new byte[pageSize];
        Flash.read(testOffset, readback, 0, pageSize);
        boolean match = true;
        for (int i = 0; i < pageSize; i++) {
            if (readback[i] != page[i]) {
                match = false;
                System.out.println("Program verify FAIL at " + i + ": expected " + (page[i] & 0xFF) + " got " + (readback[i] & 0xFF));
                break;
            }
        }
        if (match) {
            System.out.println("Program verify: PASS (256 bytes match)");
            pass++;
        } else {
            fail++;
        }

        // 6. Verify rest of sector still erased
        byte[] rest = new byte[pageSize];
        Flash.read(testOffset + pageSize, rest, 0, pageSize);
        boolean restOk = true;
        for (int i = 0; i < pageSize; i++) {
            if ((rest[i] & 0xFF) != 0xFF) {
                restOk = false;
                break;
            }
        }
        if (restOk) {
            System.out.println("Rest of sector: PASS (still 0xFF)");
            pass++;
        } else {
            System.out.println("Rest of sector: FAIL");
            fail++;
        }

        // 7. Test alignment validation
        boolean caught = false;
        try {
            Flash.erase(testOffset + 1, sectorSize); // not aligned
        } catch (IOException e) {
            caught = true;
        }
        if (caught) {
            System.out.println("Alignment check (erase): PASS");
            pass++;
        } else {
            System.out.println("Alignment check (erase): FAIL (no exception)");
            fail++;
        }

        caught = false;
        try {
            Flash.program(testOffset + 1, page, 0, pageSize); // not aligned
        } catch (IOException e) {
            caught = true;
        }
        if (caught) {
            System.out.println("Alignment check (program): PASS");
            pass++;
        } else {
            System.out.println("Alignment check (program): FAIL (no exception)");
            fail++;
        }

        // 8. Clean up - erase the test sector
        try {
            Flash.erase(testOffset, sectorSize);
            System.out.println("Cleanup erase OK");
        } catch (IOException e) {
            System.out.println("Cleanup erase failed");
        }

        System.out.println("Result: " + pass + " pass, " + fail + " fail");
        System.out.println("=== done ===");
        while (true) { SystemTimer.delayMs(5000); System.out.println("alive"); }
    }
}
