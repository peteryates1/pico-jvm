import pico.hardware.OnboardLED;
import pico.hardware.SystemTimer;

class Main {
    public static void main(String[] args) {
        System.out.println("pico-jvm starting...");
        OnboardLED led = new OnboardLED();
        int count = 0;
        while (true) {
            led.on();
            System.out.println("Hello from pico-jvm! count=" + count);
            SystemTimer.delayMs(500);
            led.off();
            SystemTimer.delayMs(500);
            count++;
        }
    }
}
