# fethub/console/syntax.py

def parse_tokens(line: str):
    """
    Divide linha em tokens, respeitando aspas.
    Ex:
        ui.text oled1 10 20 "hello world"
    vira:
        ["ui.text","oled1","10","20","hello world"]
    """

    tokens = []
    current = ""
    inside_quotes = False

    i = 0
    while i < len(line):
        c = line[i]

        if c == '"':
            inside_quotes = not inside_quotes
            i += 1
            continue

        if c == " " and not inside_quotes:
            if current:
                tokens.append(current)
                current = ""
            i += 1
            continue

        current += c
        i += 1

    if current:
        tokens.append(current)

    return tokens
