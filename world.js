export class Seg
{
    constructor(x1, y1, x2, y2, bottom, top, texname)
    {
        this.x1 = x1;
        this.y1 = y1;
        this.x2 = x2;
        this.y2 = y2;
        this.bottom = bottom;
        this.top = top;
        this.texname = texname;
    }
}

export var segs = [];