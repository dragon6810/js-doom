class Palette
{
    constructor()
    {
        // r, g, b
        this.data = [];
        this.data.length = 768;
    }
}

class Graphic
{
    constructor(w, h, offsx, offsy)
    {
        this.w = w;
        this.h = h;
        this.offsx = offsx;
        this.offsy = offsy;
        this.data = [];
        this.data.length = w * h;
    }
}

class TexPatch
{
    constructor(x, y, num)
    {
        this.x = x;
        this.y = y;
        this.num = num;
    }
}

class Texture
{
    constructor()
    {
        this.patches = [];
    }
}

var patchtextures = [];

export var palettes = [];
export var patches = []; // name-indexed

function processgraphic(data, loc)
{
    const w = data.getUint16(loc, true);
    const h = data.getUint16(loc + 2, true);
    const offsx = data.getInt16(loc + 4, true);
    const offsy = data.getInt16(loc + 6, true);

    let graphic = new Graphic(w, h, offsx, offsy);

    for(let x=0; x<w; x++)
    {
        let curloc = loc + data.getUint32(loc + 8 + 4 * x, true);
        while(true)
        {
            const ystart = data.getUint8(curloc);
            if(ystart == 255)
                break;
            
            const pixelcount = data.getUint8(curloc + 1);

            for(let y=0; y<pixelcount; y++)
            {
                const row = y + ystart;
                graphic.data[row * graphic.w + x] = data.getUint8(curloc + 3 + y);
            }

            curloc += 4 + pixelcount;
        }
    }

    return graphic;
}

function processpatch(data, loc, size, name)
{
    let graphic = processgraphic(data, loc);
    patches[name] = graphic;
}

function processpalette(data, loc, size, name)
{
    const npalettes = size / 768;
    if(Math.floor(npalettes) != npalettes)
        throw new Error("erroneous palette");

    for(let p=0; p<npalettes; p++)
    {
        let palette = new Palette();
        for(let i=0; i<768; i++)
            palette.data[i] = data.getUint8(loc + p * 768 + i);
        palettes.push(palette);
    }
}

function processtex(data, loc)
{

}

function processtexlump(data, loc, size, name)
{
    const ntextures = data.getInt32(loc, true);

    for(let i=0; i<ntextures; i++)
    {
        
    }
}

var inpatches = false;

function processlumpheader(data, headerloc)
{
    const lumploc = data.getInt32(headerloc, true);
    const size = data.getInt32(headerloc + 4, true);
    
    let name = "";
    for (let i=0; i<8; i++)
    {
        const char = data.getUint8(headerloc + 8 + i);
        if (char === 0) break;
        name += String.fromCharCode(char);
    }
    name = name.trim();

    if(name == "P_START")
        inpatches = true;
    else if(name == "P_END")
        inpatches = false;
    else if(inpatches && name != "P1_START" && name != "P1_END" && name != "P2_START" && name != "P2_END")
        processpatch(data, lumploc, size, name);
    else if(name == "TEXTURE1" || name == "TEXTURE2")
        processtexlump(data, lumploc, size, name);
    else if(name == "PLAYPAL")
        processpalette(data, lumploc, size, name);
}

function processwad(data)
{
    if(data.getUint32(0, false) != 0x49574144)
        throw new Error('unkown file type');

    const nlumps = data.getInt32(4, true);
    const tableoffs = data.getInt32(8, true);

    palettes.length = 0;
    inpatches = false;

    for(let i=0; i<nlumps; i++)
    {
        const dir = tableoffs + i * 16;
        processlumpheader(data, dir);
    }
}

export async function loadwad(name)
{
    try
    {
        const fetchresponse = await fetch('./' + name);
        if (!fetchresponse.ok)
            throw new Error(`wad fetch error: ${fetchresponse.status}`);
        
        const buffer = await fetchresponse.arrayBuffer();
        processwad(new DataView(buffer));
    }
    catch(error)
    {
        console.error("couldn't load wad: " + error);
    }
}