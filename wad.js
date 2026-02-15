class Palette
{
    constructor()
    {
        // r, g, b
        this.data = new Uint8Array(768);
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
        this.data = new Uint8Array(w * h).fill(247);
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
    constructor(w, h, ismasked)
    {
        this.w = w;
        this.h = h;
        this.ismasked = ismasked;
        this.patches = [];
        this.graphic = null;
    }
}

class Linedef
{
    constructor()
    {
        this.v1 = null;
        this.v2 = null;
        this.frontside = null;
        this.backside = null;
        this.upperunpegged = false;
        this.lowerunpegged = false;
    }
}

class Sidedef
{
    constructor()
    {
        this.xoffs = null;
        this.yoffs = null;
        this.upper = null;
        this.lower = null;
        this.middle = null;
        this.sector = null;
    }
}

class Vertex
{
    constructor(x, y)
    {
        this.x = x;
        this.y = y;
    }
}

class Seg
{
    constructor()
    {
        this.v1 = null;
        this.v2 = null;
        this.angle = null;
        this.line = null;
        this.isback = null;
        this.offset = null;

        this.ssector = null;
    }
}

class SSector
{
    constructor()
    {
        this.nseg = null;
        this.firstseg = null;
        
        this.sector = null;
    }
}

class Node
{
    constructor()
    {
        this.x = null;
        this.y = null;
        this.dx = null;
        this.dy = null;
        
        // (right, left)
        this.xmins = [ null, null, ];
        this.ymins = [ null, null, ];
        this.xmaxs = [ null, null, ];
        this.ymaxs = [ null, null, ];

        this.children = [ null, null, ];
    }
}

class Sector
{
    constructor()
    {
        this.floor = null;
        this.ceil = null;
        this.floortex = null;
        this.ceiltex = null;
    }
}

class World
{
    constructor()
    {
        this.name = null;
        this.linedefs = [];
        this.sidedefs = [];
        this.vertices = [];
        this.segs = [];
        this.ssectors = [];
        this.nodes = [];
        this.sectors = [];
    }
}

// World
export var maps = new Map();
export var textures = new Map();
export var flats = new Map();
export var palettes = [];
export var patches = new Map(); // name-indexed
export var patchnums = []; // number to name

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
    patches.set(name, graphic);
}

function processflat(data, loc, size, name)
{
    let graphic = new Graphic(64, 64, 0, 0);
    for(let i=0; i<graphic.w*graphic.w; i++)
        graphic.data[i] = data.getUint8(loc + i);

    flats.set(name, graphic);
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

function stitchtextures()
{
    textures.forEach((tex, name) =>
    {
        let graphic = new Graphic(tex.w, tex.h, 0, 0);
        tex.patches.forEach((patch) =>
        {
            const pic = patches.get(patchnums[patch.num]);
            if (!pic)
            {
                console.warn(`Texture "${name}" needs patch "${patchnums[patch.num]}" (${patch.num}), but it isn't loaded!`);
                return;
            }

            for(let y=0; y<pic.h; y++)
            {
                const desty = y + patch.y;
                if(desty < 0 || desty >= tex.h)
                    continue;

                for(let x=0; x<pic.w; x++)
                {
                    const destx = x + patch.x;
                    if(destx < 0 || destx >= tex.w)
                        continue;

                    if(pic.data[y * pic.w + x] == 247)
                        continue;

                    graphic.data[desty * graphic.w + destx] = pic.data[y * pic.w + x];
                }
            }
        });

        tex.graphic = graphic;
    });
}

function processtexlump(data, loc, size, name)
{
    const ntextures = data.getInt32(loc, true);

    for(let i=0; i<ntextures; i++)
    {
        const texoffs = loc + data.getInt32(loc + 4 + i * 4, true);

        let name = "";
        for (let i=0; i<8; i++)
        {
            const char = data.getUint8(texoffs + i);
            if (char === 0) break;
            name += String.fromCharCode(char);
        }
        name = name.trim();

        const ismasked = data.getInt32(texoffs + 8, true) != 0 ? true : false;
        const width = data.getInt16(texoffs + 12, true);
        const height = data.getInt16(texoffs + 14, true);
        
        let tex = new Texture(width, height, ismasked);

        const npatches = data.getInt16(texoffs + 20, true);
        for(let p=0; p<npatches; p++)
        {
            const patchloc = texoffs + 22 + p * 10;

            const x = data.getInt16(patchloc, true);
            const y = data.getInt16(patchloc + 2, true);
            const pnum = data.getInt16(patchloc + 4, true);

            tex.patches.push(new TexPatch(x, y, pnum));
        }

        textures.set(name, tex);
    }
}

function processpnames(data, loc, size, name)
{
    const npatches = data.getInt32(loc, true);
    for(let i=0; i<npatches; i++)
    {
        const nameloc = loc + 4 + i * 8;
        let name = "";
        for (let i=0; i<8; i++)
        {
            const char = data.getUint8(nameloc + i);
            if (char === 0) break;
            name += String.fromCharCode(char);
        }
        name = name.trim().toUpperCase();

        patchnums.push(name);
    }
}

var inpatches = false;
var inflats = false;
var maplumpcounter = 0;

var curmap = null;

function processlinedefs(data, name, loc, size)
{
    if(name != "LINEDEFS")
        throw new Error('map ' + curmap.name + ' lacks or has out of order LINEDEFS lump');

    const nlinedefs = size / 14;
    curmap.linedefs.length = nlinedefs;

    for(let i=0; i<nlinedefs; i++)
    {
        const lineloc = loc + i * 14;

        let line = new Linedef();

        line.v1 = data.getInt16(lineloc, true);
        line.v2 = data.getInt16(lineloc + 2, true);
        line.frontside = data.getInt16(lineloc + 10, true);
        line.backside = data.getInt16(lineloc + 12, true);

        const flags = data.getInt16(lineloc + 4, true);
        if(flags & 0x0008)
            line.upperunpegged = true;
        if(flags & 0x0010)
            line.lowerunpegged = true;

        curmap.linedefs[i] = line;
    }
}

function parsestring8(data, loc)
{
    let str = "";
    for (let i=0; i<8; i++)
    {
        const char = data.getUint8(loc + i);
        if (char === 0) break;
        str += String.fromCharCode(char);
    }
    return str.trim();
}

function processsidedefs(data, name, loc, size)
{
    if(name != "SIDEDEFS")
        throw new Error('map ' + curmap.name + ' lacks or has out of order SIDEDEFS lump');

    const nsidedefs = size / 30;
    curmap.sidedefs.length = nsidedefs;

    for(let i=0; i<nsidedefs; i++)
    {
        const sideloc = loc + i * 30;

        let side = new Sidedef();

        side.xoffs = data.getInt16(sideloc, true);
        side.yoffs = data.getInt16(sideloc + 2, true);
        side.upper = parsestring8(data, sideloc + 4);
        side.lower = parsestring8(data, sideloc + 12);
        side.middle = parsestring8(data, sideloc + 20);
        side.sector = data.getInt16(sideloc + 28, true);

        curmap.sidedefs[i] = side;
    }
}

function processvertices(data, name, loc, size)
{
    if(name != "VERTEXES")
        throw new Error('map ' + curmap.name + ' lacks or has out of order VERTEXES lump');

    const nverts = size / 4;
    curmap.vertices.length = nverts;

    for(let i=0; i<nverts; i++)
    {
        const vertloc = loc + i * 4;

        let vert = new Vertex();

        vert.x = data.getInt16(vertloc, true);
        vert.y = data.getInt16(vertloc + 2, true);

        curmap.vertices[i] = vert;
    }
}

function processsegs(data, name, loc, size)
{
    if(name != "SEGS")
        throw new Error('map ' + curmap.name + ' lacks or has out of order SEGS lump');

    const nsegs = size / 12;
    curmap.segs.length = nsegs;

    for(let i=0; i<nsegs; i++)
    {
        const segloc = loc + i * 12;

        let seg = new Seg();

        seg.v1 = data.getInt16(segloc, true);
        seg.v2 = data.getInt16(segloc + 2, true);
        seg.angle = data.getUint16(segloc + 4, true) * Math.PI * 2 / 0xFFFF;
        seg.line = data.getInt16(segloc + 6, true);
        seg.isback = data.getInt16(segloc + 8, true) == 0 ? false : true;
        seg.offset = data.getInt16(segloc + 10, true);

        curmap.segs[i] = seg;
    }
}

function processssectors(data, name, loc, size)
{
    if(name != "SSECTORS")
        throw new Error('map ' + curmap.name + ' lacks or has out of order SSECTORS lump');

    const nssectors = size / 4;
    curmap.ssectors.length = nssectors;

    for(let i=0; i<nssectors; i++)
    {
        const sectorloc = loc + i * 4;

        let sector = new SSector();

        sector.nseg = data.getInt16(sectorloc, true);
        sector.firstseg = data.getInt16(sectorloc + 2, true);

        curmap.ssectors[i] = sector;
    }
}

function processnodes(data, name, loc, size)
{
    if(name != "NODES")
        throw new Error('map ' + curmap.name + ' lacks or has out of order NODES lump');

    const nnodes = size / 28;
    curmap.nodes.length = nnodes;

    for(let i=0; i<nnodes; i++)
    {
        const nodeloc = loc + i * 28;

        let node = new Node();

        node.x = data.getInt16(nodeloc, true);
        node.y = data.getInt16(nodeloc + 2, true);
        node.dx = data.getInt16(nodeloc + 4, true);
        node.dy = data.getInt16(nodeloc + 6, true);

        node.ymaxs[0] = data.getInt16(nodeloc + 8, true);
        node.ymins[0] = data.getInt16(nodeloc + 10, true);
        node.xmins[0] = data.getInt16(nodeloc + 12, true);
        node.xmaxs[0] = data.getInt16(nodeloc + 14, true);
        node.ymaxs[1] = data.getInt16(nodeloc + 16, true);
        node.ymins[1] = data.getInt16(nodeloc + 18, true);
        node.xmins[1] = data.getInt16(nodeloc + 20, true);
        node.xmaxs[1] = data.getInt16(nodeloc + 22, true);
        
        node.children[0] = data.getInt16(nodeloc + 24, true);
        node.children[1] = data.getInt16(nodeloc + 26, true);

        curmap.nodes[i] = node;
    }
}

function processsectors(data, name, loc, size)
{
    if(name != "SECTORS")
        throw new Error('map ' + curmap.name + ' lacks or has out of order SECTORS lump');

    const nsectors = size / 26;
    curmap.sectors.length = nsectors;

    for(let i=0; i<nsectors; i++)
    {
        const sectorloc = loc + i * 26;

        let sector = new Sector();

        sector.floor = data.getInt16(sectorloc, true);
        sector.ceil = data.getInt16(sectorloc + 2, true);
        sector.floortex = parsestring8(data, sectorloc + 4);
        sector.ceiltex = parsestring8(data, sectorloc + 12);

        curmap.sectors[i] = sector;
    }
}

function finishmap()
{
    for(let ss=0; ss<curmap.ssectors.length; ss++)
    {
        let subsector = curmap.ssectors[ss];
        for(let s=subsector.firstseg; s<subsector.firstseg+subsector.nseg; s++)
        {
            let seg = curmap.segs[s];
            seg.subsector = ss;
            
            const linedef = curmap.linedefs[seg.line];
            const sidedef = curmap.sidedefs[seg.isback ? linedef.backside : linedef.frontside];
            subsector.sector = sidedef.sector;
            // console.log(ss + ' -> ' + curmap.ssectors[ss].sector);
        }
    }
}

function processmaplump(data, name, loc, size)
{
    switch(maplumpcounter)
    {
    case 1:
        break;
    case 2:
        processlinedefs(data, name, loc, size);
        break;
    case 3:
        processsidedefs(data, name, loc, size)
        break;
    case 4:
        processvertices(data, name, loc, size)
        break;
    case 5:
        processsegs(data, name, loc, size);
        break;
    case 6:
        processssectors(data, name, loc, size);
        break;
    case 7:
        processnodes(data, name, loc, size);
        break;
    case 8:
        processsectors(data, name, loc, size);
        break;
    case 9:
        break;
    case 10:
        break;
    default:
        break;
    }

    maplumpcounter++;
    if(maplumpcounter > 10)
    {
        finishmap();
        maps.set(curmap.name, curmap);
        maplumpcounter = 0;
    }
}

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

    if(maplumpcounter != 0)
        processmaplump(data, name, lumploc, size);
    else if(/^E\dM\d$/.test(name))
    {
        curmap = new World();
        curmap.name = name;
        maplumpcounter = 1;
    }
    else if(name == "P_START")
        inpatches = true;
    else if(name == "P_END")
        inpatches = false;
    else if(inpatches && name != "P1_START" && name != "P1_END" && name != "P2_START" && name != "P2_END")
        processpatch(data, lumploc, size, name);
    else if(name == "F_START")
        inflats = true;
    else if(name == "F_END")
        inflats = false;
    else if(inflats && name != "F1_START" && name != "F1_END" && name != "F2_START" && name != "F2_END")
        processflat(data, lumploc, size, name);
    else if(name == "TEXTURE1" || name == "TEXTURE2")
        processtexlump(data, lumploc, size, name);
    else if(name == "PNAMES")
        processpnames(data, lumploc, size, name);
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

    stitchtextures();
}

export function nodeside(node, x, y)
{
    const dx = x - node.x;
    const dy = y - node.y;


    if((node.dy * dx) - (node.dx * dy) > 0)
        return 0;
    return 1;
}

// returns index pf sector
export function getpointsector(map, x, y)
{
    let nodenum = map.nodes.length - 1;
    while(nodenum >= 0)
    {
        const node = map.nodes[nodenum];
        const side = nodeside(node, x, y);
    
        nodenum = node.children[side];
    }

    return map.ssectors[nodenum & 0x7FFF].sector;
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