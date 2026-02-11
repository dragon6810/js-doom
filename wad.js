export async function loadwad(name)
{
    try
    {
        const fetchresponse = await fetch('./' + name);
        if (!fetchresponse.ok)
            throw new Error(`wad fetch error: ${fetchresponse.status}`);
        
        const buffer = await fetchresponse.arrayBuffer();
        const bytes = new Uint8Array(buffer);

        console.log("File loaded. Total bytes:", bytes.length);
        console.log("First 4 bytes:", bytes.slice(0, 4));
    }
    catch(error)
    {
        console.error("couldn't load wad: " + error);
    }
}