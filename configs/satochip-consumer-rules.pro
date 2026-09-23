-keep interface com.nunchuk.android.satochip.SatochipCard { *; }
-keepclassmembers class * implements com.nunchuk.android.satochip.SatochipCard {
    public *;
}
