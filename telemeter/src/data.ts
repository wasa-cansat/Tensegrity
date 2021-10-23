var fs = require("fs");

const SerialPort = require("chrome-apps-serialport").SerialPort

export class DataSet {
    data:       Array<any>                = []
    incomings:  { [seq: number]: any }    = []
    seqMapping: { [seq: number]: number } = {}
    latestSeq?: number                    = undefined

    parsers: { [type_: string]: Parser<any> }
    keyType: string

    text: string = ''
    serialBuffer: string = ''

    onUpdateCallback: (newText: string) => void = () => {}

    constructor(parsers: Array<Parser<any>>, keyType: string) {
        this.parsers = Object.fromEntries(parsers.map(p => [p.type_, p]))
        this.keyType = keyType
        this.clear()
    }

    count(): number {
        return this.data.length
    }

    clear() {
        this.data         = []
        this.incomings    = []
        this.seqMapping   = {}
        this.latestSeq    = undefined
        this.text         = ''
        this.serialBuffer = ''
    }

    getLatest(offset?: number): any {
        return this.data[(this.data.length - 1 + (offset || 0))]
            || this.data[this.data.length - 1]
    }

    getAt(t: number): any {
        return this.data[t]
    }

    writeHexToFile(path: string) {
        fs.writeFile(path, this.text + '\n', (err: any) => {
            if (err)  {
                console.error(err)
                return
            }
            console.log('Saved hex file')
        })
    }
    readHexFromFile(path: string) {
        this.clear()
        fs.readFile(path, 'utf8' , (err: any, text: string) => {
            if (err) {
                console.error(err)
                return
            }
            console.log('Read hex file')
            this.text = text
            const messages = text.split('\n')
            for (const message of messages) this.appendHexMessage(message)
            console.log(this.data)
            this.onUpdateCallback(text)
        })
    }
    readHexFromSerial(path: string) {
        const serialport =
            new SerialPort(
                path, {baudrate: 115200},
                (err: any) => {
                    if (err) console.error(err.message)
                })
        this.serialBuffer = ''
        serialport.on('data', (d: any) => {
            const text = d.toString()
            this.serialBuffer += text
            const messages = this.serialBuffer.split('\n')
            if (messages.length > 0) this.serialBuffer = messages.pop()!
            for (const message of messages) this.appendHexMessage(message)
            this.onUpdateCallback(text)
        })
    }

    onUpdate(callback: (newText: string) => void) {
        this.onUpdateCallback = callback
    }

    appendHexMessage(hexes: string) {
        const message = this.parseHex(hexes)
        if (!message) return
        this.append(message)
        if (message.type_ == this.keyType) this.keySequence(message.seq)
    }

    append(m: Message) {
        if (this.latestSeq === undefined) this.latestSeq = m.seq
        if (this.incomings[m.seq] === undefined)
            this.incomings[m.seq] = {}
        if (m.index == null) this.incomings[m.seq][m.type_] = m.payload
        else {
            if (this.incomings[m.seq][m.type_] === undefined)
                this.incomings[m.seq][m.type_] = {}
            this.incomings[m.seq][m.type_][m.index] = m.payload
        }
    }
    keySequence(seq: number) {
        // if (this.seqMapping[seq] >= this.data.length) return
        this.seqMapping[seq] = this.data.length
        this.data.push(this.incomings[seq])
        this.incomings[(seq - 128) % 256] = {}
        this.latestSeq = seq
    }
    parseHex(hexes?: string): Message | null {
        if (!hexes || hexes[0] == '#') return null
        const bytes = hexes.match(/.{1,2}/g)!.map(s => parseInt(s, 16))
        if (bytes.some(isNaN)) return null

        const id    = String.fromCharCode(bytes[0])
        const seq   = bytes[1]
        const type_ = String.fromCharCode(bytes[2])
        const index = bytes[3]

        const parser = this.parsers[type_]
        if (!parser) return null

        const view = new DataView(new ArrayBuffer(4))
        for (let i = 0; i < 4; i++) view.setUint8(i, bytes[i+4])
        const payload = parser.payloadReader(view)

        const index_ = (parser.indices || [])[index] || null

        return new Message(id, seq, type_, index_, payload)
    }

    printHex(message: Message): string | null {
        const parser = this.parsers[message.type_]
        if (!parser) return null
        const view = new DataView(new ArrayBuffer(4))
        parser.payloadWriter(view, message.payload)
        const bytes = new Array(8)

        bytes[0] = message.id.charCodeAt(0)
        bytes[1] = message.seq
        bytes[2] = message.type_.charCodeAt(0)
        bytes[3] = message.index
        for (let i = 0; i < 4; i++) bytes[i+4] = view.getUint8(i)

        return null
    }
}

export class Parser<T> {
    constructor(public type_: string,
                public payloadReader: (view: DataView) => T,
                public payloadWriter: (view: DataView, payload: T) => void,
                public indices?: Array<string>) {}
    static float(type_: string, indices?: Array<string>): Parser<number> {
        return new Parser(type_,
                          (view) => view.getFloat32(0, true),
                          (view, payload) => view.setFloat32(0, payload, true),
                          indices)
    }
    static int(type_: string, indices?: Array<string>): Parser<number> {
        return new Parser(type_,
                          (view) => view.getInt32(0, true),
                          (view, payload) => view.setInt32(0, payload, true),
                          indices)
    }
}

class Message {
    constructor(public id: string,
                public seq: number,
                public type_: string,
                public index: string | null,
                public payload: any) {}
}
