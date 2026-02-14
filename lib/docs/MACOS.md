# Running Sil-Q on macOS

## macOS native app (Cocoa) with Xcode

When you launch `Sil.app` for the first time, the [Gatekeeper security
feature](https://support.apple.com/en-us/102445) in macOS 10.15 or later may
prevent you from starting or using the app.

To solve this problem, follow these three steps:

1. If you see a macOS dialog window "Sil.app Not Opened" with a note about Apple
   not being able to verify that the app is free of malware, then click the button
   "Done". (Do not click on "Move to Trash".)
2. Then go to your Mac's `System Settings` > `Privacy & Security`, and scroll
   all the way down to the `Security` section, which will have an entry "Sil.app
   was blocked to protect your Mac". Here, click on the button "Open Anyway" and
   confirm that you do want to launch Sil-Q.
3. When `Sil.app` launches now, you will see another macOS dialog window about
   granting Sil-Q access to your `Documents` folder. You must approve this
   access, because it is required by Sil-Q to store its savefiles for your
   in-game characters, the high score file, and additional game data in the
   folder `Documents/Sil/`.

Enjoy!

> If you know how to use the macOS terminal, you can alternatively run
> `xattr -rc /path/to/your/Sil.app` to complete steps 1 and 2 above.
