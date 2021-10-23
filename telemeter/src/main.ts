import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls'

import 'fomantic-ui'
import { DataSet, Parser } from './data'

const SerialPort = require("chrome-apps-serialport").SerialPort



window.addEventListener('load', () => {

    console.log('Start telemeter')

    let live = true
    let autoPlayTimer: number | undefined

    const dataSet = new DataSet(
        [Parser.float('T'),
         Parser.float('L', ['latitude, longitude']),
         Parser.float('Q', ['a', 'b', 'c', 'd']),
         Parser.float('A', ['x', 'y', 'z']),
         Parser.float('G', ['x', 'y', 'z']),
         new Parser('R',
                    (view: DataView) => ({
                        target:   view.getInt8(0),
                        position: view.getInt8(1),
                        voltage:  view.getInt8(2),
                        fault:    view.getUint8(3)}),
                    (view: DataView, r) => {
                        view.setInt8(0, r.target)
                        view.setInt8(1, r.position)
                        view.setInt8(2, r.voltage)
                        view.setInt8(3, r.fault)
                    },
                    ['0', '1', '2', '3', '4', '5'])
        ], 'T')

    dataSet.onUpdate(onread)

    $('#source-dropdown').dropdown({
        onChange: async (_, text: string) => {
            switch (text) {
                case 'File':
                    $('#port-dropdown').hide()
                    $('#load-file-input').trigger('click')
                    break
                case 'Serial':
                    const list = await SerialPort.list()
                    console.log(list)
                    $('#file-path').hide()
                    $('#port-dropdown')
                            .show()
                            .dropdown({
                                values:
                                list.filter((port: any) => port.manufacturer)
                                    .map((port: any) =>
                                        ({name: port.comName,
                                          value: port.path})),
                                onChange: (path: string) => {
                                    dataSet.readHexFromSerial(path)
                                }
                            })
                    break
            }

        }
    })

    $('#save-button')
        .on('click', () => {$('#save-file-input').trigger('click')})
    $('#clear-button').on('click', () => {
        dataSet.clear()
        $('#log-screen').empty()
        $('#state-screen').empty()
    })

    $('#save-file-input').on('input', (e: any) => {
        const file = e.target.files[0]
        dataSet.writeHexToFile(file.path)
        $(e.target).prop('value', '')
    })

    $('#load-file-input').on('input', (e: any) => {
        const file = e.target.files[0]
        dataSet.readHexFromFile(file.path)
        $('#file-path').show().text(file.path)
        $(e.target).prop('value', '')
    })

    { ($('#time-slider') as any).slider(
        {max: 0, onChange: (t: any) => {
            live = false
            showData(dataSet.getAt(t))
        }})
    }
    $('#backward-button').on('click', () => {
        live = false
        { ($('#time-slider') as any).slider('set value', 0, false) }
        showData(dataSet.getAt(0))
    })
    $('#pause-button').on('click', () => {
        live = false
        window.clearTimeout(autoPlayTimer)
        autoPlayTimer = undefined
    })
    $('#play-button').on('click', () => {
        live = false
        const t = ($('#time-slider') as any).slider('get value') as number
        autoPlay(t)
    })
    $('#forward-button').on('click', () => {
        live = true
        { ($('#time-slider') as any)
              .slider('set value', dataSet.count() - 1, false) }
        showData(dataSet.getLatest(-1))
    })

    function autoPlay(t: number) {
        ($('#time-slider') as any).slider('set value', t, false)
        showData(dataSet.getAt(t))
        if (t < dataSet.count() - 1) {
            autoPlayTimer = window.setTimeout(() => {
                autoPlay(t + 1)
            }, (dataSet.getAt(t+1).T - dataSet.getAt(t).T) * 1000.0)
        }
    }


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

    const goalVec =
        new THREE.ArrowHelper(
            new THREE.Vector3(0,0,0), new THREE.Vector3(0,0,0), 1, 0xFF0000)
    body.add(goalVec)

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
        update(dataSet: any) {
            this.runner.position.y = dataSet.position
            this.target.position.y = dataSet.target
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

    function showData(data: any) {
        $('#state-screen').text(JSON.stringify(data, null, '  '))
        $('#time-label').text((data.T >= 0 ? '+' : '') + data.T)

        const Q = data.Q || {a: 1, b: 0, c: 0, d: 0}
        const q = new THREE.Quaternion(Q.b, Q.c, Q.d, Q.a)
        console.log(q, q.conjugate())
        // q.invert()
        // console.log(q)
        body.setRotationFromQuaternion(q.conjugate())

        const A = data.A || {x: 0, y: 0, z: -1}
        const a = new THREE.Vector3(A.x, A.y, A.z)
        accelVec.setDirection(a)

        const G = data.A || {x: 0, y: 0, z: 0}
        const g = new THREE.Vector3(G.x, G.y, G.z)
        accelVec.setDirection(g)

        if (data.R)
            for (const [i, leg] of legs.entries()) leg.update(data.R[i])

    }

    function onread(newText: string) {
        if ($('#log-screen')[0].scrollHeight > 10000) $('#log-screen').empty()
        $('#log-screen')
            .append(newText)
            .animate({scrollTop: $('#log-screen')[0].scrollHeight}, 0)

        { (($('#time-slider')) as any)
              .slider('setting', 'max', dataSet.count() - 1) }

        const latest = dataSet.getLatest(-1)
        if (!latest) return
        if (live) {
            (($('#time-slider')) as any)
                  .slider('set value', dataSet.count(), false)
            showData(latest)
        }
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
