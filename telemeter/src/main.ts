import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls'

// const serialport = require('serialport')
// const bindings = require('@serialport/bindings')
// import * as bindings from '@serialport/list'

const SerialPort = require("chrome-apps-serialport").SerialPort;

window.addEventListener('DOMContentLoaded', () => {

    console.log('Start telemeter')


    var listOfPorts=[];

    //called automatically by bindings.list()
    function list(ports: any) {
        listOfPorts = ports;
        // now listOfPorts will be the port Objects
        console.log(listOfPorts);
    }
    SerialPort.list().then(list)


    const scene = new THREE.Scene()
    scene.background = new THREE.Color(0x202020)

    const camera = new THREE.PerspectiveCamera(45, 640/480, 1, 10000)
    camera.position.set(5,2,5)

    const mainRenderer: THREE.WebGLRenderer = new THREE.WebGLRenderer()
    const mainView = document.getElementById('main-view')!
    mainView.appendChild(mainRenderer.domElement)

    window.addEventListener('resize', onResize)

    function onResize() {
        const width  = mainView.clientWidth;
        const height = mainView.clientHeight;

        mainRenderer.setPixelRatio(window.devicePixelRatio);
        mainRenderer.setSize(width, height);

        camera.aspect = width / height;
        camera.updateProjectionMatrix();
    }
    onResize()



    const controls = new OrbitControls(camera, mainRenderer.domElement)
    controls.enableZoom = true
    controls.target = new THREE.Vector3(0, 0, 0)


    const grid = new THREE.GridHelper(20, 20)
    grid.position.y = -1
    scene.add(grid)


    const body = new THREE.Object3D()
    scene.add(body)

    const axes = new THREE.AxesHelper(1);
    body.add(axes);

    const legs = []
    for (let i = 0; i < 6; i++) {
        const g = new THREE.CylinderGeometry(0.02, 0.02, 2, 6)
        const m = new THREE.MeshPhongMaterial({color: 0xffffff})
        const leg = new THREE.Mesh(g, m)
        body.add(leg)
        legs.push(leg)
    }
    const offset = 0.6
    legs[0].position.x = offset
    legs[1].position.x = - offset
    legs[2].position.y = offset
    legs[3].position.y = - offset
    legs[4].position.z = offset
    legs[5].position.z = - offset

    legs[0].rotateX(Math.PI / 2)
    legs[1].rotateX(Math.PI / 2)
    legs[2].rotateZ(Math.PI / 2)
    legs[3].rotateZ(Math.PI / 2)

    const light = new THREE.DirectionalLight(0xffffff)
    light.position.set(1,1,1)
    scene.add(light)


    const tick = ():void => {
        requestAnimationFrame(tick)

        controls.update()
        mainRenderer.render(scene, camera)
    }
    tick()
})
