/* Trimpod: a minimal on-screen keyboard (see trimpod_keyboard.c).
 *
 * An iPod-aesthetic grid keyboard rendered in ChicagoFLF: A-Z (case via an
 * on-screen capslock), 0-9, hyphen, space, backspace and enter.  Replaces the
 * stock Rockbox kbd_input for Trimpod text entry. */
#ifndef _TRIMPOD_KEYBOARD_H
#define _TRIMPOD_KEYBOARD_H

/* Edit `buffer` (NUL-terminated, capacity `buflen`) with the on-screen keyboard.
 * Returns 0 when the on-screen enter key is engaged, -1 on cancel (B).  On
 * cancel `buffer` is left as it was passed in. */
int trimpod_kbd_input(char *buffer, int buflen);

#endif /* _TRIMPOD_KEYBOARD_H */
