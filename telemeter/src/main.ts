import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls'

import 'fomantic-ui'
import { DataSet, Parser } from './data'

const SerialPort = require("chrome-apps-serialport").SerialPort



window.addEventListener('load', () => {

    console.log('Start telemeter')

    let serialport: typeof SerialPort
    let telemetryBuffer: string = ''

    const data = new DataSet(
        [Parser.float('T'),
         Parser.float('L', ['latitude, longitude']),
         Parser.float('Q', ['a', 'b', 'c', 'd']),
         Parser.float('A', ['x', 'y', 'z']),
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

    data.onUpdate(onread)

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
                                    data.readHexFromSerial(path)
                                }
                            })
                    break
            }

        }
    })

    $('#save-button')
        .on('click', () => {$('#save-file-input').trigger('click')})
    $('#clear-button').on('click', () => {
        data.clear()
        $('#log-screen').empty()
        $('#state-screen').empty()
    })

    $('#save-file-input').on('input', (e: any) => {
        const file = e.target.files[0]
        data.writeHexToFile(file.path)
        $(e.target).prop('value', '')
    })

    $('#load-file-input').on('input', (e: any) => {
        const file = e.target.files[0]
        data.readHexFromFile(file.path)
        $('#file-path').show().text(file.path)
        $(e.target).prop('value', '')
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


    function onread(newText: string) {
        if ($('#log-screen')[0].scrollHeight > 10000) $('#log-screen').empty()
        $('#log-screen')
            .append(newText)
            .animate({scrollTop: $('#log-screen')[0].scrollHeight}, 0)

        const latest = data.getLatest(-1)
        if (!latest) return
        $('#state-screen').text(JSON.stringify(latest, null, '  '))

        const q = new THREE.Quaternion(
            latest.Q.b, latest.Q.c, latest.Q.d, latest.a)
        console.log(q)
        // q.invert()
        // console.log(q)
        body.setRotationFromQuaternion(q.conjugate())
        const a = new THREE.Vector3(latest.A.x, latest.A.y, latest.A.z)
        accelVec.setDirection(a)
        for (const [i, leg] of legs.entries()) leg.update(latest.R[i])
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
