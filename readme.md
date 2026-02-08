# PassGen – Human‑Readable Password Generator

PassGen generates memorable, client‑appropriate passwords using random words and numbers.
It supports an **optional enhanced security mode** ("pepper") without making normal usage complicated.

This tool works on **Windows, macOS, and Linux**.

Note: Prebuilt Binaries are available for Windows and Linux. Mac users can build from source.

---

## Files Included

- `passgen` / `passgen.exe` – the password generator executable
- `words.txt` – word list used to generate passwords
- `README.md` – this file

Keep `passgen` and `words.txt` in the same directory.

---

## Basic Usage (Recommended for Shared / Client Passwords)

Run the program with no arguments:

```bash
./passgen
```

Example output:

```
Evergreen-Seedling-0206
Tailfeather-Rice-5645
```

### Defaults

- 2 words
- 4‑digit number
- 5 passwords generated
- Capitalization enabled
- No logging
- No secrets required

This mode works immediately on all platforms.

---

## Common Options

```
--count N        Number of passwords to generate (default: 5)
--words N        Number of words per password (default: 2)
--no-capitalize  Disable word capitalization
--log <file>     Explicitly log generated passwords to a file (use with care)
--help           Show help
```

Example:

```bash
./passgen --count 10 --words 3
```

---

## Enhanced Security Mode (Pepper)

Pepper mode adds a **4‑character secret‑derived tag** to each password.
This prevents attackers from reproducing passwords even if they know the wordlist and format.

Example:

```
Stew-Meteor-1963-6ICR
```

Pepper mode is **explicit** and **opt‑in**.

---

## Using Pepper Mode

To use pepper mode:

1. Set the environment variable `PASSGEN_PEPPER`
2. Run PassGen with the `--pepper` flag

If `--pepper` is set and `PASSGEN_PEPPER` is not defined, the program will exit with an error.

---

## Setting `PASSGEN_PEPPER`

### Linux / macOS (temporary – current shell)

```bash
export PASSGEN_PEPPER="your-secret-string"
./passgen --pepper
```

### Linux / macOS (`.env` file)

Create a file named `.env` in the same directory as `passgen`:

```
PASSGEN_PEPPER=your-secret-string
```

Then run:

```bash
./passgen --pepper
```

> The `.env` file is optional and should **never** be shared or committed to source control.

---

### Windows (PowerShell)

Temporary (current window):

```powershell
$env:PASSGEN_PEPPER="your-secret-string"
.\passgen.exe --pepper
```

Persistent (per user):

```powershell
[Environment]::SetEnvironmentVariable(
  "PASSGEN_PEPPER",
  "your-secret-string",
  "User"
)
```

Open a **new PowerShell window**, then:

```powershell
.\passgen.exe --pepper
```

---

### Windows (Command Prompt)

Temporary:

```bat
set PASSGEN_PEPPER=your-secret-string
passgen.exe --pepper
```

Persistent:

```bat
setx PASSGEN_PEPPER "your-secret-string"
```

Open a new command prompt before running.

---

## Important Notes

- Pepper mode is best for **personal or high‑risk passwords**
- Do **not** share your pepper with others
- Do **not** rely on pepper mode for passwords that must be jointly regenerated
- Logging is **disabled by default** for safety
- Generated passwords are only logged if `--log` is explicitly provided

---

## Example Workflows

Basic client password:

```bash
./passgen --words 2
```

Personal password (enhanced):

```bash
PASSGEN_PEPPER=secret ./passgen --pepper --words 3
```

Bulk generation:

```bash
./passgen --count 20
```

---

## Security Philosophy

This tool prioritizes:

- Memorability over complexity theater
- Explicit risk over silent defaults
- No shared secrets
- No hidden logging
- Predictable, auditable behavior

---

## License / Distribution

This tool is intended for internal or trusted use.
Review and adapt policies as needed before distributing to clients.

