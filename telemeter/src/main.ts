import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls'

import 'fomantic-ui'

const SerialPort = require("chrome-apps-serialport").SerialPort

class DataSet {
    data:       Array<any>                = []
    incomings:  { [seq: number]: any }    = []
    seqMapping: { [seq: number]: number } = {}
    latestSeq?: number                    = undefined

    append(seq: number, title: string, value: any, index?: number, ) {
        if (this.latestSeq === undefined) this.latestSeq = seq
        if (this.incomings[seq] === undefined) this.incomings[seq] = {}
        if (index === undefined) this.incomings[seq][title] = value
        else {
            if (this.incomings[seq][title] === undefined)
                this.incomings[seq][title] = []
            this.incomings[seq][title][index] = value
        }
    }
    keySequence(seq: number) {
        // if (this.seqMapping[seq] >= this.data.length) return
        this.seqMapping[seq] = this.data.length
        this.data.push(this.incomings[seq])
        this.incomings[(seq - 128) % 256] = {}
        this.latestSeq = seq
    }
    getLatest(offset?: number): any {
        return this.data[(this.data.length - 1 + (offset || 0))]
    }
}

window.addEventListener('load', () => {

    console.log('Start telemeter')

    let serialport: typeof SerialPort
    let telemetryBuffer: string = ''

    const data = new DataSet()

    let onDataUpdate = (_: any) => {}

    function onread(d: any) {
        // const data = serialport.read()
        const str = d.toString()
        // console.log(str)
        if ($('#log-screen')[0].scrollHeight > 10000) $('#log-screen').empty()
        $('#log-screen')
            .append(str)
            .animate({scrollTop: $('#log-screen')[0].scrollHeight}, 0)
        telemetryBuffer += str
        const commands = telemetryBuffer.split('\n')
        if (commands.length > 0) telemetryBuffer = commands.pop()!
        for (const command of commands) {
            if (command[0] == '#' || !command) continue
            const hexes = command.match(/.{1,2}/g)!.map(s => parseInt(s, 16))
            if (hexes.some(isNaN)) continue
            const view = new DataView(new ArrayBuffer(4))
            for (let i = 0; i < 4; i++) view.setUint8(3 - i, hexes[i+4])
            const id    = String.fromCharCode(hexes[0])
            const seq   = hexes[1]
            const type_ = String.fromCharCode(hexes[2])
            const index = hexes[3]

            let fvalue = view.getFloat32(0)
            let ivalue = view.getInt32(0)
            switch (type_) {
                case 'T':
                    data.append(seq, type_, fvalue)
                    data.keySequence(seq)
                    break
                case 'Q':
                case 'L':
                case 'A':
                    data.append(seq, type_, fvalue, index)
                    break
                case 'R':
                    const d = {target: view.getInt8(3),
                               position: view.getInt8(2),
                               voltage: view.getInt8(1),
                               fault: view.getUint8(0)}
                    data.append(seq, type_, d, index)
                    break
                default:
                    continue
            }
        }

        const latest = data.getLatest(-1)

        if (latest)  {
            $('#state-screen').text(JSON.stringify(latest, null, '  '))
            onDataUpdate(latest)
        }
    }

    $('#source-dropdown').dropdown({
        onChange: async (_, text: string) => {
            switch (text) {
                case 'Serial':
                    const list = await SerialPort.list()
                    console.log(list)
                    $('#port-dropdown').dropdown({
                        values: list.filter((port: any) =>
                            port.manufacturer).map((port: any) =>
                                ({name: port.comName, value: port.path})),
                        onChange: (path: string) => {
                            serialport =
                                new SerialPort(
                                    path,
                                    {baudrate: 115200},
                                    (err: any) => {
                                        if (err) console.log('Error: ',
                                                             err.message)
                                    })
                            serialport.on('open',
                                          () => {serialport.drain()})
                            serialport.on('data', onread)
                        }
                    })
                    break
            }

        }
    })

    THREE.Object3D.DefaultUp.set(0, 0, 1);

    const scene = new THREE.Scene()
    scene.background = new THREE.Color(0x202020)

    const mainView = document.getElementById('main-view')!
    const mainCamera = new THREE.PerspectiveCamera(45, 1, 1, 10000)
    mainCamera.position.set(5, 5, 2)
    const mainRenderer = new THREE.WebGLRenderer()
    mainView.appendChild(mainRenderer.domElement)
    const mainControls = new OrbitControls(mainCamera, mainRenderer.domElement)
    mainControls.enableZoom = true
    mainControls.target = new THREE.Vector3(0, 0, 0)

    const skyView = document.getElementById('sky-view')!
    const skyCamera = new THREE.PerspectiveCamera(45, 1, 1, 10000)
    skyCamera.position.set(0, 0, 20)
    skyCamera.lookAt(new THREE.Vector3(0, 0, 0))
    const skyRenderer = new THREE.WebGLRenderer()
    skyView.appendChild(skyRenderer.domElement)
    const skyControls = new OrbitControls(skyCamera, skyRenderer.domElement)
    skyControls.enableZoom = true
    skyControls.enableRotate = false
    // skyControls.target = new THREE.Vector3(0, 0, 0)

    const bodyView = document.getElementById('body-view')!
    const bodyCamera = new THREE.PerspectiveCamera(45, 1, 1, 10000)
    bodyCamera.position.set(5,5,2)
    bodyCamera.lookAt(new THREE.Vector3(0, 0, 0))
    const bodyRenderer = new THREE.WebGLRenderer()
    bodyView.appendChild(bodyRenderer.domElement)
    const bodyControls = new OrbitControls(bodyCamera, bodyRenderer.domElement)
    bodyControls.enableZoom = true


    window.addEventListener('resize', onResize)
    onResize()

    function onResize() {
        resizeView(mainView, mainRenderer, mainCamera)
        resizeView(skyView, skyRenderer, skyCamera)
        resizeView(bodyView, bodyRenderer, bodyCamera)
    }
    function resizeView(view: HTMLElement,
                        renderer: THREE.WebGLRenderer,
                        camera: THREE.PerspectiveCamera) {
        const width  = view.clientWidth
        const height = view.clientHeight

        renderer.setPixelRatio(window.devicePixelRatio)
        renderer.setSize(width, height)

        camera.aspect = width / height
        camera.updateProjectionMatrix()
    }

    const grid = new THREE.GridHelper(20, 20)
    grid.rotateX(Math.PI / 2)
    grid.position.z = -1
    scene.add(grid)

    const axes = new THREE.AxesHelper(1);
    scene.add(axes);

    const body = new THREE.Object3D()
    scene.add(body)
    body.add(bodyCamera)

    const bodyAxes = new THREE.AxesHelper(1)
    body.add(bodyAxes)

    const accelVec =
        new THREE.ArrowHelper(
            new THREE.Vector3(0,0,-1), new THREE.Vector3(0,0,0), 1, 0xFFFFFF)
    body.add(accelVec)

    const pointGeometry = new THREE.BufferGeometry()
    pointGeometry.setAttribute('position',
                           new THREE.Float32BufferAttribute([0, 0, 0], 3))
    const pointMaterial = new THREE.PointsMaterial( { color: 0xFFFFFF } )
    pointMaterial.size = 5
    pointMaterial.sizeAttenuation = false
    const point = new THREE.Points(pointGeometry, pointMaterial)
    body.add(point);

    class Leg {
        offset: number = 0.6
        mesh:   THREE.Mesh
        runner: THREE.Mesh
        target: THREE.Mesh
        constructor(body: THREE.Object3D) {
            this.mesh =
                new THREE.Mesh(
                    new THREE.CylinderGeometry(0.02, 0.02, 2, 6),
                    new THREE.MeshPhongMaterial({color: 0xffffff}))
            body.add(this.mesh)

            const gr = new THREE.CylinderGeometry(0.1, 0.1, 0.2, 10)
            const mr = new THREE.MeshPhongMaterial({color: 0xffff00})
            this.runner = new THREE.Mesh(gr, mr)
            this.mesh.add(this.runner)

            const gt = new THREE.SphereGeometry(0.1, 8, 8)
            const mt = new THREE.MeshPhongMaterial({color: 0xff0000})
            this.target = new THREE.Mesh(gt, mt)
            this.mesh.add(this.target)
        }
        update(data: any) {
            this.runner.position.y = data.position
            this.target.position.y = data.target
        }
    }

    const legs: Array<Leg> = []
    for (let i = 0; i < 6; i++) {
        legs.push(new Leg(body))
    }
    const offset = 0.6
    legs[0].mesh.position.x = offset
    legs[1].mesh.position.x = - offset
    legs[2].mesh.position.y = offset
    legs[3].mesh.position.y = - offset
    legs[4].mesh.position.z = offset
    legs[5].mesh.position.z = - offset

    legs[0].mesh.rotateX(Math.PI / 2)
    legs[1].mesh.rotateX(Math.PI / 2)
    legs[2].mesh.rotateZ(Math.PI / 2)
    legs[3].mesh.rotateZ(Math.PI / 2)

    const light = new THREE.DirectionalLight(0xffffff)
    light.position.set(1,1,1)
    scene.add(light)


    onDataUpdate = (data: any) => {
        const q = new THREE.Quaternion(
            data.Q[0], data.Q[1], data.Q[2], data.Q[3])
        console.log(q)
        // q.invert()
        // console.log(q)
        body.setRotationFromQuaternion(q.conjugate())
        const a = new THREE.Vector3(data.A[0], data.A[1], data.A[2])
        accelVec.setDirection(a)
        for (const [i, leg] of legs.entries()) leg.update(data.R[i])
    }

    const tick = ():void => {
        requestAnimationFrame(tick)

        mainControls.update()
        skyControls.update()
        bodyControls.update()
        mainRenderer.render(scene, mainCamera)
        skyRenderer.render(scene, skyCamera)
        bodyRenderer.render(scene, bodyCamera)
    }
    tick()
})
