import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import FluentUI
import Utils

Item{
    id: root
    property alias model : deviceListView.model
    readonly property var itemWidth: [150.00, 120.00, 140.00, 90.00, 70.00, 140.00, 120.00]
    readonly property int itemTotalWidth: itemWidth.reduce((acc, cur) => acc + cur, 0)
    // 分页配置
    // property int pageSize: 10
    // property int currentPage: 1
    // readonly property int totalCount: deviceListView.count
    // readonly property int totalPages: Math.max(1, Math.ceil(totalCount / pageSize))
    // readonly property int pageStartIndex: (currentPage - 1) * pageSize
    // readonly property int pageEndIndex: Math.min(pageStartIndex + pageSize, totalCount)
    signal clickMenuItem(var model)
    
    // 启动 scrcpy_server（TCP直连模式需要先启动服务）
    function startScrcpyServerForDevice(hostIp, dbId, tcpVideoPort, tcpAudioPort, tcpControlPort, onSuccess, onError) {
        if (!hostIp || !dbId) {
            if (onError) onError("hostIp 或 dbId 为空")
            return
        }
        
        const url = `http://${hostIp}:18182/container_api/v1/scrcpy`
        console.log("启动 scrcpy_server:", url, "dbId:", dbId, "videoPort:", tcpVideoPort, "audioPort:", tcpAudioPort, "controlPort:", tcpControlPort)
        
        // 根据端口判断是否需要启动对应的流
        const bool_video = tcpVideoPort > 0
        const bool_audio = tcpAudioPort > 0
        const bool_control = tcpControlPort > 0
        
        Network.postJson(url)
        .bind(root)
        .setTimeout(5000)
        .add("db_id", dbId)
        .add("bool_video", bool_video)
        .add("bool_audio", bool_audio)
        .add("bool_control", bool_control)
        .go(startScrcpyServerCallable)
        
        // 保存回调函数
        startScrcpyServerCallable._onSuccess = onSuccess
        startScrcpyServerCallable._onError = onError
        startScrcpyServerCallable._hostIp = hostIp
        startScrcpyServerCallable._dbId = dbId
    }
    
    NetworkCallable {
        id: startScrcpyServerCallable
        property var _onSuccess: null
        property var _onError: null
        property string _hostIp: ""
        property string _dbId: ""
        
        onError: (status, errorString, result, userData) => {
            console.error("启动 scrcpy_server 失败:", status, errorString, result)
            if (_onError) {
                _onError(errorString || "启动 scrcpy_server 失败")
            }
        }
        
        onSuccess: (result, userData) => {
            var res = JSON.parse(result)
            if (res.code === 200) {
                console.log("启动 scrcpy_server 成功:", result)
                if (_onSuccess) {
                    _onSuccess(_hostIp, _dbId)
                }
            } else {
                console.error("启动 scrcpy_server 失败:", res.msg)
                if (_onError) {
                    _onError(res.msg || "启动 scrcpy_server 失败")
                }
            }
        }
    }

    ColumnLayout{
        anchors.fill: parent
        spacing: 0

        Rectangle{
            Layout.preferredHeight: 40
            Layout.fillWidth: true
            color: "white"
            border.width: 1
            border.color: "#eee"
            topLeftRadius: 8
            topRightRadius: 8

            RowLayout{
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 0

                VCheckBox{
                    id: headerPageCheck
                    checked: proxyModel.isSelectAll
                    // enabled: !groupControl.eventSync
                    onClicked: {
                        if(checkIsEventSync()){
                            return
                        }

                        checkBoxInvertSelection.checked = false
                        proxyModel.selectAll(checked)
                    }
                    // 监听模型选择变更与分页变更，实时刷新头部全选状态
                    // Connections{
                    //     target: proxyModel
                    //     function onCheckedCountChanged(){
                    //         headerPageCheck.pageAllChecked = proxyModel.isAllCheckedInRange(root.pageStartIndex, root.pageEndIndex)
                    //     }
                    //     function onIsSelectAllChanged(){
                    //         headerPageCheck.pageAllChecked = proxyModel.isAllCheckedInRange(root.pageStartIndex, root.pageEndIndex)
                    //     }
                    // }
                    // Connections{
                    //     target: root
                    //     function onCurrentPageChanged(){
                    //         headerPageCheck.pageAllChecked = proxyModel.isAllCheckedInRange(root.pageStartIndex, root.pageEndIndex)
                    //     }
                    //     function onPageSizeChanged(){
                    //         headerPageCheck.pageAllChecked = proxyModel.isAllCheckedInRange(root.pageStartIndex, root.pageEndIndex)
                    //     }
                    // }
                }

                Item{
                    Layout.fillHeight: true
                    Layout.preferredWidth: deviceListView.width * itemWidth[0] / itemTotalWidth

                    FluText{
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("云机ID")
                    }
                }

                Item{
                    Layout.fillHeight: true
                    Layout.preferredWidth: deviceListView.width * itemWidth[1] / itemTotalWidth


                    FluText{
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("云机名称")
                    }
                }

                Item{
                    Layout.fillHeight: true
                    Layout.preferredWidth: deviceListView.width * itemWidth[2] / itemTotalWidth


                    FluText{
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("ADB地址")
                    }
                }

                Item{
                    Layout.fillHeight: true
                    Layout.preferredWidth: deviceListView.width * itemWidth[3] / itemTotalWidth


                    FluText{
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Android版本")
                    }
                }
                Item{
                    Layout.fillHeight: true
                    Layout.preferredWidth: deviceListView.width * itemWidth[4] / itemTotalWidth

                    FluText{
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("状态")
                    }
                }
                Item{
                    Layout.fillHeight: true
                    Layout.preferredWidth: deviceListView.width * itemWidth[5] / itemTotalWidth


                    FluText{
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("创建时间")
                    }
                }
                Item{
                    Layout.fillHeight: true
                    Layout.preferredWidth: deviceListView.width * itemWidth[6] / itemTotalWidth

                    FluText{
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("操作")
                    }
                }
                // Item{
                //     Layout.fillWidth: true
                // }
            }
        }

        ListView{
            id: deviceListView
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }
            interactive: true
            
            // 当数据量变化时修正当前页
            // Connections{
            //     target: deviceListView
            //     function onCountChanged(){
            //         if(root.currentPage > root.totalPages){
            //             root.currentPage = root.totalPages
            //         }
            //         if(root.currentPage < 1){
            //             root.currentPage = 1
            //         }
            //     }
            // }
            // 使用委托内覆盖层实现悬停高亮，避免与行内控件层级冲突

            function formatDateTimeRaw(isoString) {
                return isoString.replace("T", " ").replace("Z", "");
            }

            delegate: Rectangle {
                width: deviceListView.width
                height: 40
                color: "transparent"
                // readonly property bool inPage: index >= root.pageStartIndex && index < root.pageEndIndex

                // 顶层覆盖层：始终在所有内容之上
                Rectangle {
                    anchors.fill: parent
                    color: mouseArea.containsMouse ? "#26000000" : "transparent"
                    z: 999
                    // visible: inPage
                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    // visible: inPage
                    onEntered: deviceListView.currentIndex = index
                    onExited: if (deviceListView.currentIndex === index) deviceListView.currentIndex = -1
                }

                ColumnLayout{
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    // visible: inPage


                    RowLayout{
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 0

                        VCheckBox{
                            checked: model?.checked ?? false
                            onClicked: {
                                if(model.checked != checked){
                                    model.checked = checked
                                }
                            }
                        }

                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: deviceListView.width * itemWidth[0] / itemTotalWidth

                            FluText{
                                anchors.verticalCenter: parent.verticalCenter
                                text: model?.dbId ?? ""

                                MouseArea{
                                    anchors.fill: parent

                                    onClicked: {
                                        FluTools.clipText(model?.dbId ?? "")
                                        showSuccess(qsTr("复制成功"))
                                    }
                                }
                            }
                        }

                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: deviceListView.width * itemWidth[1] / itemTotalWidth


                            FluText{
                                anchors.verticalCenter: parent.verticalCenter
                                text: model?.displayName ?? ""

                                MouseArea{
                                    anchors.fill: parent

                                    onClicked: {
                                        FluTools.clipText(model?.displayName ?? "")
                                        showSuccess(qsTr("复制成功"))
                                    }
                                }
                            }
                        }

                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: deviceListView.width * itemWidth[2] / itemTotalWidth

                            FluText{
                                anchors.verticalCenter: parent.verticalCenter
                                text: (model?.hostIp ?? "") + ":" + (model?.adb ?? "")

                                MouseArea{
                                    anchors.fill: parent

                                    onClicked: {
                                        FluTools.clipText((model?.hostIp ?? "") + ":" + (model?.adb ?? ""))
                                        showSuccess(qsTr("复制成功"))
                                    }
                                }
                            }
                        }

                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: deviceListView.width * itemWidth[3] / itemTotalWidth

                            Rectangle{
                                width: 78
                                height: 28
                                color: "#E7F0FF"
                                anchors.verticalCenter: parent.verticalCenter
                                radius: 14

                                FluText {
                                    anchors.centerIn: parent
                                    text: "Android " + (model?.aospVersion ?? "")
                                    font.pixelSize: 12
                                    color: ThemeUI.primaryColor
                                }
                            }
                        }
                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: deviceListView.width * itemWidth[4] / itemTotalWidth

                            Rectangle{
                                width: statusText.implicitWidth + 16
                                height: statusText.implicitHeight + 8
                                border.color: AppUtils.getStateColorBystate(model.state).border
                                border.width: 1
                                color: AppUtils.getStateColorBystate(model.state).bg
                                anchors.verticalCenter: parent.verticalCenter
                                radius: 2

                                FluText {
                                    id: statusText
                                    anchors.centerIn: parent
                                    text: AppUtils.getStateStringBystate(model?.state ?? "")
                                    color: AppUtils.getStateColorBystate(model.state).text
                                    font.pixelSize: 12
                                }
                            }
                        }

                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: deviceListView.width * itemWidth[5] / itemTotalWidth

                            FluText{
                                anchors.verticalCenter: parent.verticalCenter
                                text: deviceListView.formatDateTimeRaw(model?.created ?? "")
                            }
                        }

                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: deviceListView.width * itemWidth[6] / itemTotalWidth

                            RowLayout{
                                anchors.fill: parent

                                TextButton {
                                    text: qsTr("开机")
                                    visible: (model.state === "exited" || model.state === "stopped")
                                    Layout.preferredHeight: 32
                                    Layout.preferredWidth: 72
                                    borderRadius: 4
                                    backgroundColor: ThemeUI.primaryColor
                                    Layout.alignment: Qt.AlignHCenter
                                    onClicked: {
                                        if(model.state !== "exited" && model.state !== "stopped"){
                                            return
                                        }
                                        reqRunDevice(model.hostIp, [model.dbId])
                                    }
                                }

                                TextButton {
                                    text: qsTr("打开窗口")
                                    visible: model.state === "running"
                                    Layout.preferredHeight: 32
                                    Layout.preferredWidth: 72
                                    borderRadius: 4
                                    backgroundColor: ThemeUI.primaryColor
                                    Layout.alignment: Qt.AlignHCenter
                                    onClicked: {
                                        if(model.state !== "running"){
                                            return
                                        }

                                        // 根据配置选择连接模式
                                        const hostIp = model.hostIp || ""
                                        const adb = model.adb || 0
                                        const dbId = model.dbId || model.db_id || model.id || model.name || ""
                                        
                                    if (AppConfig.useDirectTcp) {
                                        // TCP直接连接模式：先启动 scrcpy_server，再连接
                                        const tcpVideoPort = model.tcpVideoPort || 0
                                        const tcpAudioPort = model.tcpAudioPort || 0
                                        const tcpControlPort = model.tcpControlPort || 0
                                        
                                        console.log("使用TCP直接连接模式:", hostIp, dbId, "ports:", tcpVideoPort, tcpAudioPort, tcpControlPort)
                                        
                                        // 先启动 scrcpy_server
                                        // startScrcpyServerForDevice(hostIp, dbId, tcpVideoPort, tcpAudioPort, tcpControlPort,
                                        //     // 启动成功后的回调
                                        //     (hostIp, dbId) => {
                                                console.log("scrcpy_server 启动成功，开始连接设备")
                                                deviceManager.connectDeviceDirectTcp(
                                                    dbId,           // serial
                                                    hostIp || "localhost",  // host
                                                    tcpVideoPort,   // videoPort
                                                    tcpAudioPort,   // audioPort
                                                    tcpControlPort  // controlPort
                                                )
                                        //     },
                                        //     // 启动失败的回调
                                        //     (error) => {
                                        //         console.error("启动 scrcpy_server 失败，无法连接设备:", error)
                                        //     }
                                        // )
                                    } else {
                                        // ADB连接模式
                                        const deviceAddress = `${hostIp}:${adb}`
                                        console.log("使用ADB连接模式:", deviceAddress)
                                        deviceManager.connectDevice(deviceAddress)
                                    }
                                        
                                        FluRouter.navigate("/pad", model, undefined, model.id)
                                    }
                                }

                                FluIcon{
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    visible: model.state === "running"
                                    iconSource: FluentIcons.More
                                    color: ThemeUI.primaryColor
                                    iconSize: 14
                                    rotation: 90

                                    MouseArea{
                                        anchors.fill: parent
                                        onClicked: {
                                            clickMenuItem(model)
                                        }
                                    }
                                }

                                Item{
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                    FluDivider{
                        Layout.fillWidth: true
                    }
                }
            }
        }
        
        // 底部分页条容器（无数据时不显示）
        // Item{
        //     Layout.fillWidth: true
        //     Layout.preferredHeight: 32
        //     Layout.leftMargin: 10
        //     Layout.rightMargin: 10
        //     Layout.bottomMargin: 10
        //     visible: root.totalCount > 0
        //     // radius: 8
        //     // color: "white"
        //     // border.color: "#E5E5E5"
        //     // border.width: 1

        //     Item{
        //         anchors.fill: parent
        //         anchors.leftMargin: 16
        //         anchors.rightMargin: 16
        //         Row{
        //             anchors.right: parent.right
        //             anchors.verticalCenter: parent.verticalCenter
        //             spacing: 12

        //             FluPagination{
        //                 id: pagination
        //                 itemCount: root.totalCount
        //                 pageCurrent: root.currentPage
        //                 __itemPerPage: root.pageSize
        //                 pageButtonCount: 7
        //                 previousText: "<"
        //                 nextText: ">"
        //                 footer: Row {
        //                     spacing: 8

        //                     FluComboBox{
        //                         id: pageSizeCombo
        //                         width: 110
        //                         height: 32
        //                         model: [10, 20, 30, 50]
        //                         // 展开列表背景改为白色
        //                         popup.background: Rectangle { color: "white"; border.color: "#E5E5E5"; radius: 4 }
        //                         delegate: ItemDelegate {
        //                             width: parent ? parent.width : 110
        //                             background: Rectangle { color: (hovered || highlighted) ? "#F5F5F5" : "white" }
        //                             contentItem: Text {
        //                                 text: modelData + qsTr("条/页")
        //                                 horizontalAlignment: Text.AlignHCenter
        //                                 verticalAlignment: Text.AlignVCenter
        //                                 color: "#222222"
        //                             }
        //                             onClicked: {
        //                                 pageSizeCombo.currentIndex = index
        //                                 pageSizeCombo.popup.close()
        //                             }
        //                         }
        //                         contentItem: Text {
        //                             text: (pageSizeCombo.currentIndex >= 0 ? (pageSizeCombo.model[pageSizeCombo.currentIndex] + qsTr("条/页")) : (root.pageSize + qsTr("条/页")))
        //                             verticalAlignment: Text.AlignVCenter
        //                             horizontalAlignment: Text.AlignHCenter
        //                             elide: Text.ElideRight
        //                         }
        //                         Component.onCompleted: {
        //                             let idx = model.indexOf(root.pageSize)
        //                             currentIndex = idx >= 0 ? idx : 0
        //                         }
        //                         onCurrentIndexChanged: {
        //                             const newSize = model[currentIndex]
        //                             if (root.pageSize !== newSize) {
        //                                 root.pageSize = newSize
        //                                 root.currentPage = 1
        //                             }
        //                         }
        //                     }
        //                 }
        //                 onRequestPage: function(page, count){
        //                     root.currentPage = page
        //                     if (root.pageSize !== count) {
        //                         root.pageSize = count
        //                     }
        //                 }
        //             }
        //         }
        //     }
        // }
    }
}

