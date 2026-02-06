const pressedkeys = {};

export function inputinit()
{
    window.addEventListener('keydown', (event) => {
        pressedkeys[event.code] = true;
    });
    
    window.addEventListener('keyup', (event) => {
        pressedkeys[event.code] = false;
    });
}

export function iskeydown(key)
{
    return pressedkeys[key] == true;
}