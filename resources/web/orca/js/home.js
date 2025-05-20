/*var TestData={"sequence_id":"0","command":"studio_send_recentfile","data":[{"path":"D:\\work\\Models\\Toy\\3d-puzzle-cube-model_files\\3d-puzzle-cube.3mf","time":"2022\/3\/24 20:33:10"},{"path":"D:\\work\\Models\\Art\\Carved Stone Vase - remeshed+drainage\\Carved Stone Vase.3mf","time":"2022\/3\/24 17:11:51"},{"path":"D:\\work\\Models\\Art\\Kity & Cat\\Cat.3mf","time":"2022\/3\/24 17:07:55"},{"path":"D:\\work\\Models\\Toy\\鐩村墤.3mf","time":"2022\/3\/24 17:06:02"},{"path":"D:\\work\\Models\\Toy\\minimalistic-dual-tone-whistle-model_files\\minimalistic-dual-tone-whistle.3mf","time":"2022\/3\/22 21:12:22"},{"path":"D:\\work\\Models\\Toy\\spiral-city-model_files\\spiral-city.3mf","time":"2022\/3\/22 18:58:37"},{"path":"D:\\work\\Models\\Toy\\impossible-dovetail-puzzle-box-model_files\\impossible-dovetail-puzzle-box.3mf","time":"2022\/3\/22 20:08:40"}]};*/

var m_HotModelList = null;

function OnInit() {
  //-----Test-----
  //Set_RecentFile_MouseRightBtn_Event();

  //-----Official-----
  TranslatePage();

  SendMsg_GetLoginInfo();
  SendMsg_GetRecentFile();
  SendMsg_GetStaffPick();

  //InitStaffPick();
}

//------最佳打开文件的右键菜单功能----------
var RightBtnFilePath = "";

var MousePosX = 0;
var MousePosY = 0;
var sImages = {};

function Set_RecentFile_MouseRightBtn_Event() {
  $(".FileItem").mousedown(function (e) {
    //FilePath
    RightBtnFilePath = $(this).attr("fpath");

    if (e.which == 3) {
      //鼠标点击了右键+$(this).attr('ff') );
      ShowRecnetFileContextMenu();
    } else if (e.which == 2) {
      //鼠标点击了中键
    } else if (e.which == 1) {
      //鼠标点击了左键
      OnOpenRecentFile(encodeURI(RightBtnFilePath));
    }
  });

  $(document).bind("contextmenu", function (e) {
    //在这里书写代码，构建个性右键化菜单
    return false;
  });

  $(document).mousemove(function (e) {
    MousePosX = e.pageX;
    MousePosY = e.pageY;

    let ContextMenuWidth = $("#recnet_context_menu").width();
    let ContextMenuHeight = $("#recnet_context_menu").height();

    let DocumentWidth = $(document).width();
    let DocumentHeight = $(document).height();

    //$("#DebugText").text( ContextMenuWidth+' - '+ContextMenuHeight+'<br/>'+
    //					 DocumentWidth+' - '+DocumentHeight+'<br/>'+
    //					 MousePosX+' - '+MousePosY +'<br/>' );
  });

  $(document).click(function () {
    var e = e || window.event;
    var elem = e.target || e.srcElement;
    while (elem) {
      if (elem.id && elem.id == "recnet_context_menu") {
        return;
      }
      elem = elem.parentNode;
    }

    $("#recnet_context_menu").hide();
  });
}

function HandleStudio(pVal) {
  let strCmd = pVal["command"];

  if (strCmd == "get_recent_projects") {
    ShowRecentFileList(pVal["response"]);
  } else if (strCmd == "studio_userlogin") {
    SetLoginInfo(pVal["data"]["avatar"], pVal["data"]["name"]);
  } else if (strCmd == "modify_rtsp_player_address") {
    SetUrlInfo(pVal["address"], pVal["language"]);
  } else if (strCmd == "close_rtsp") {
    SetClose();
  } else if (strCmd == "studio_useroffline") {
    SetUserOffline();
  } else if (strCmd == "studio_set_mallurl") {
    SetMallUrl(pVal["data"]["url"]);
  } else if (strCmd == "studio_clickmenu") {
    let strName = pVal["data"]["menu"];

    GotoMenu(strName);
  } else if (strCmd == "network_plugin_installtip") {
    let nShow = pVal["show"] * 1;

    if (nShow == 1) {
      $("#NoPluginTip").show();
      $("#NoPluginTip").css("display", "flex");
    } else {
      $("#NoPluginTip").hide();
    }
  } else if (strCmd == "modelmall_model_advise_get") {
    //alert('hot');
    if (m_HotModelList != null) {
      let SS1 = JSON.stringify(pVal["hits"]);
      let SS2 = JSON.stringify(m_HotModelList);

      if (SS1 == SS2) return;
    }

    m_HotModelList = pVal["hits"];
    ShowStaffPick(m_HotModelList);
  }
}

function GotoMenu(strMenu) {
  let MenuList = $(".BtnItem");
  let nAll = MenuList.length;

  for (let n = 0; n < nAll; n++) {
    let OneBtn = MenuList[n];

    if ($(OneBtn).attr("menu") == strMenu) {
      $(".BtnItem").removeClass("BtnItemSelected");

      $(OneBtn).addClass("BtnItemSelected");

      $("div[board]").hide();
      $("div[board='" + strMenu + "']").show();
    }
  }
}

function SetLoginInfo(strAvatar, strName) {
  $("#Login1").hide();

  $("#UserName").text(strName);

  let OriginAvatar = $("#UserAvatarIcon").prop("src");
  if (strAvatar != OriginAvatar) {
    if (strAvatar != "default.jpg") {
      $("#UserAvatarIcon").prop("src", strAvatar);
    }
  } else {
    //alert('Avatar is Same');
  }

  $("#Login2").show();
  $("#Login2").css("display", "flex");
}

var flag= false;
var flag1= false;

function SetUrlInfo(strAddress, strLanguage) {
  window.strAddress = strAddress;
  $("#url-studio").text(strAddress);
  $("#url-studio-r").text(strLanguage);
  var loader = document.getElementById('videoLoader');
  let lang = strAddress;

  // var platform = window.navigator.platform;
  // var userAgent = window.navigator.userAgent;

  var point = document.getElementById("point");
  var point1 = document.getElementById("point1");

  const isMac = /macintosh|mac os x/i.test(navigator.userAgent);

  let zting = document.querySelector('#zanting')
  if (lang) {
    if (lang.endsWith(".m3u8")) {
      video.style.display = "block";
      videoStreamImg.style.display = "none";
      video.style.backgroundImage = "url('hei.svg')";
      video.style.backgroundSize = "cover";
      video.style.backgroundRepeat = "no-repeat";
      video.style.backgroundPosition = "center";

      var handleClick = function () {
        if (!streamPaused) {
          OnGetUrl();
          setTimeout(function () {
            var element = document.getElementById("url-studio");
            var content = element.innerHTML;
            lang = content;

            var element1 = document.getElementById("url-studio-r");
            var content1 = element1.innerHTML;
            lang1 = content1;
            if (lang === '设备离线了') {
              if (lang1 === 'zh_CN') {
                point.style.display = 'block'
                setTimeout(function() {
                  point.style.display = 'none'
                },3000)
              } else if (lang1 === 'en') {
                point1.style.display = 'block'
                setTimeout(function() {
                  point1.style.display = 'none'
                },3000)
              }
            } else {
              if (isMac) {
                if (video.canPlayType("application/vnd.apple.mpegurl")) {
                  if (!streamPaused) {
                    // 对于原生支持 HLS 的平台（如 Safari），直接播放
                    video.style.backgroundImage = "none";
                    var zanting = document.getElementById("zanting");
                    zanting.style.display = "none";
                    video.src = lang;
    
                    // 当视频被点击，检查其播放状态
                    if (video.readyState < 2) {
                        // 如果视频未加载足够的数据，则显示加载动画
                        loader.style.display = 'block';
                        // 尝试播放视频
                        video.play().catch(function(e) {
                            console.error("播放失败:", e);
                        });
                    }
  
                    // 设置一个标志来跟踪视频是否开始播放
                    var isVideoStarting = false;
                    
                    function hideLoader() {
                      isVideoStarting = true;
                      loader.style.display = 'none';
                    }
    
                  // 当视频开始播放时，隐藏加载动画
                  video.addEventListener('playing', hideLoader);
    
                  // 视频暂停时，不显示加载动画
                  video.addEventListener('pause', hideLoader);
                
                  // 视频发生错误时显示加载动画
                  video.addEventListener('error', hideLoader);
  
                  // 设置一个超时时间，在30秒后检查视频是否开始播放
                  setTimeout(function() {
                    if (!isVideoStarting) {
                      loader.style.display = 'none';
                      video.src = "";
                      video.style.backgroundImage = "url('hei.svg')";
                      video.style.backgroundSize = "cover";
                      video.style.backgroundRepeat = "no-repeat";
                      video.style.backgroundPosition = "center";
                      var zanting = document.getElementById("zanting");
                      zanting.style.display = "block";
                      streamPaused = false;
                      if (lang1 === 'zh_CN') {
                        point.style.display = 'block'
                        setTimeout(function() {
                          point.style.display = 'none'
                        },3000)
                      } else if (lang1 === 'en') {
                        point1.style.display = 'block'
                        setTimeout(function() {
                          point1.style.display = 'none'
                        },3000)
                      }
                    }
                  }, 30000); // 30秒超时时间
  
  
                    setTimeout(function () {
                      video.src = "";
                      video.style.backgroundImage = "url('hei.svg')";
                      video.style.backgroundSize = "cover";
                      video.style.backgroundRepeat = "no-repeat";
                      video.style.backgroundPosition = "center";
                      var zanting = document.getElementById("zanting");
                      zanting.style.display = "block";
                      streamPaused = false;
                    }, 300000); // 10分钟后暂停，10分钟 = 600000毫秒
                    streamPaused = true;
                  }
                }
              } else {
                if (Hls.isSupported()) {
                  // var video = document.getElementById('video')
                  video.style.backgroundImage = "none";
                  var zanting = document.getElementById("zanting");
                  zanting.style.display = "none";
                  var hls = new Hls();
                  // 绑定视频流地址
                  hls.loadSource(lang);
                  // 绑定 video 容器
                  hls.attachMedia(video);
                  if (!streamPaused) {
                    // 当视频被点击，检查其播放状态
                    if (video.readyState < 2) {
                        // 如果视频未加载足够的数据，则显示加载动画
                        loader.style.display = 'block';
                        // setTimeout(function() {
                        //   loader.style.display = 'none';
                        //   video.src = "";
                        //   video.style.backgroundImage = "url('hei.svg')";
                        //   video.style.backgroundSize = "cover";
                        //   video.style.backgroundRepeat = "no-repeat";
                        //   video.style.backgroundPosition = "center";
                        //   var zanting = document.getElementById("zanting");
                        //   zanting.style.display = "block";
                        //   streamPaused = false;
                        //   point.style.display = 'block'
                        //   setTimeout(function() {
                        //     point.style.display = 'none'
                        //   },3000)
                        // },30000)
                        // 尝试播放视频
                        video.play().catch(function(e) {
                            console.error("播放失败:", e);
                        });
                    }
  
                    // 设置一个标志来跟踪视频是否开始播放
                    var isVideoStarting = false;
                    
                    function hideLoader() {
                      isVideoStarting = true;
                      loader.style.display = 'none';
                    }
    
                    // 当视频开始播放时，隐藏加载动画
                    video.addEventListener('playing', hideLoader);
                    // 视频暂停时，不显示加载动画
                    video.addEventListener('pause', hideLoader);
                
                    // 视频发生错误时显示加载动画
                    video.addEventListener('error', hideLoader);
  
                    // 设置一个超时时间，在30秒后检查视频是否开始播放
                    setTimeout(function() {
                      if (!isVideoStarting) {
                        loader.style.display = 'none';
                        video.src = "";
                        video.style.backgroundImage = "url('hei.svg')";
                        video.style.backgroundSize = "cover";
                        video.style.backgroundRepeat = "no-repeat";
                        video.style.backgroundPosition = "center";
                        var zanting = document.getElementById("zanting");
                        zanting.style.display = "block";
                        streamPaused = false;
                        if (lang1 === 'zh_CN') {
                          point.style.display = 'block'
                          setTimeout(function() {
                            point.style.display = 'none'
                          },3000)
                        } else if (lang1 === 'en') {
                          point1.style.display = 'block'
                          setTimeout(function() {
                            point1.style.display = 'none'
                          },3000)
                        }
                      }
                    }, 30000); // 30秒超时时间
    
                    setTimeout(function () {
                      video.src = "";
                      video.style.backgroundImage = "url('hei.svg')";
                      video.style.backgroundSize = "cover";
                      video.style.backgroundRepeat = "no-repeat";
                      video.style.backgroundPosition = "center";
                      var zanting = document.getElementById("zanting");
                      zanting.style.display = "block";
                      streamPaused = false;
                    }, 300000); // 10分钟后暂停，10分钟 = 600000毫秒
                    streamPaused = true;
                  }
                }
              }
            }
          }, 10);
        } else {
          // video.pause();
          video.src = ''
          video.style.backgroundImage = "url('hei.svg')";
          video.style.backgroundSize = "cover";
          video.style.backgroundRepeat = "no-repeat";
          video.style.backgroundPosition = "center";
          var zanting = document.getElementById("zanting");
          zanting.style.display = "block";
          streamPaused = false;
        }
      };


	  if (!flag) {
      flag = true;
      zting.removeEventListener("click", handleClick);
      zting.addEventListener("click", handleClick);
		  video.removeEventListener("click", handleClick);
		  video.addEventListener("click", handleClick);
	  }
      
    } else {
      // flag = false;
      // alert(flag)
      // video.style.display = 'none'
      // videoStreamImg.src = 'hei.svg'
      // videoStreamImg.style.backgroundImage = 'url("hei.svg")'
      var handleClicktr = function () {
        if (!streamPaused) {
          OnGetUrl();
          setTimeout(function () {
            var element = document.getElementById("url-studio");
            var content = element.innerHTML;
            lang = content;

            var element1 = document.getElementById("url-studio-r");
            var content1 = element1.innerHTML;
            lang1 = content1;
            if (lang === '设备离线了') {
              if (lang1 === 'zh_CN') {
                point.style.display = 'block'
                setTimeout(function() {
                  point.style.display = 'none'
                },3000)
              } else if (lang1 === 'en') {
                point1.style.display = 'block'
                setTimeout(function() {
                  point1.style.display = 'none'
                },3000)
              }
            } else {
              loader.style.display = 'block'
              // alert(lang)
              videoStreamImg.src = "";
              videoStreamImg.style.backgroundImage = 'url("")';
              var zanting = document.getElementById("zanting");
              zanting.style.display = "none";
              videoStreamImg.src = lang;

              // videoStream.addEventListener('loadstart', function() {
              //   loader.style.display = 'block'
              //   setTimeout(function() {
              //     loader.style.display = 'none';
              //     videoStreamImg.src = ''
              //     // loader.style.display === 'none'
              //     videoStreamImg.src = 'hei.svg'
              //     var zanting = document.getElementById("zanting")
              //     zanting.style.display = 'block'
              //     videoStreamImg.style.backgroundImage = 'url("hei.svg")'
              //     streamPaused = false;
              //     point.style.display = 'block'
              //     setTimeout(function() {
              //       point.style.display = 'none'
              //     },3000)
              //   },30000)
              // });

              // 设置一个标志来跟踪视频是否开始播放
              var isVideoStarting = false;

              videoStream.addEventListener('load', function() {
                  isVideoStarting = true;
                  // 隐藏加载动画
                  videoLoader.style.display = 'none';
              }, { once: true }); // 使用 { once: true } 参数，确保事件只触发一次
  
              videoStream.addEventListener('error', function() {
                  isVideoStarting = true;
                  // 隐藏加载动画
                  videoLoader.style.display = 'none';
              });

              // 设置一个超时时间，在30秒后检查视频是否开始播放
              setTimeout(function() {
                if (!isVideoStarting) {
                  loader.style.display = 'none';
                  videoStreamImg.src = ''
                  videoStreamImg.src = 'hei.svg'
                  var zanting = document.getElementById("zanting")
                  zanting.style.display = 'block'
                  videoStreamImg.style.backgroundImage = 'url("hei.svg")'
                  streamPaused = false;
                  if (lang1 === 'zh_CN') {
                    point.style.display = 'block'
                    setTimeout(function() {
                      point.style.display = 'none'
                    },3000)
                  } else if (lang1 === 'en') {
                    point1.style.display = 'block'
                    setTimeout(function() {
                      point1.style.display = 'none'
                    },3000)
                  }
                }
              }, 30000); // 30秒超时时间


              streamPaused = true;
              setTimeout(function () {
                videoStreamImg.src = "hei.svg";
                zanting.style.display = "block";
                videoStreamImg.style.backgroundImage = 'url("hei.svg")';
                streamPaused = false;
              }, 300000); // 10分钟后暂停，10分钟 = 600000毫秒
            }
          }, 10);
        } else {
          videoStreamImg.src = "hei.svg";
          var zanting = document.getElementById("zanting");
          zanting.style.display = "block";
          videoStreamImg.style.backgroundImage = 'url("hei.svg")';
          streamPaused = false;
        }
      }
      
      if (!flag1) {
        flag1 = true;
        for (let i = 0; i < imgs.length; i++) {
          imgs[i].removeEventListener("click", handleClicktr);
          imgs[i].addEventListener("click", handleClicktr);
        }
      }
    }
  } else {
    video.style.display = "none";
    videoStreamImg.src = "hei.svg";
    videoStreamImg.style.backgroundImage = 'url("hei.svg")';
    for (let i = 0; i < imgs.length; i++) {
      imgs[i].addEventListener("click", function () {
        alert("无法获取视频流路径");
      });
    }
  }
}

function SetClose() {
  $("#url-studio").text("设备离线了");
}

function SetUserOffline() {
  $("#UserAvatarIcon").prop("src", "img/c.jpg");
  $("#UserName").text("");
  $("#Login2").hide();

  $("#Login1").show();
  $("#Login1").css("display", "flex");
}

function SetMallUrl(strUrl) {
  $("#MallWeb").prop("src", strUrl);
}

function ShowRecentFileList(pList) {
  let nTotal = pList.length;

  let strHtml = "";
  for (let n = 0; n < nTotal; n++) {
    let OneFile = pList[n];

    let sPath = OneFile["path"];
    let sImg = OneFile["image"] || sImages[sPath];
    let sTime = OneFile["time"];
    let sName = OneFile["project_name"];
    sImages[sPath] = sImg;

    //let index=sPath.lastIndexOf('\\')>0?sPath.lastIndexOf('\\'):sPath.lastIndexOf('\/');
    //let sShortName=sPath.substring(index+1,sPath.length);

    let TmpHtml =
      '<div class="FileItem"  fpath="' +
      sPath +
      '"  >' +
      '<a class="FileTip" title="' +
      sPath +
      '"></a>' +
      '<div class="FileImg" ><img src="' +
      sImg +
      '" onerror="this.onerror=null;this.src=\'img/d.png\';"  alt="No Image"  /></div>' +
      '<div class="FileName TextS1">' +
      sName +
      "</div>" +
      '<div class="FileDate">' +
      sTime +
      "</div>" +
      "</div>";

    strHtml += TmpHtml;
  }

  $("#FileList").html(strHtml);

  Set_RecentFile_MouseRightBtn_Event();
  UpdateRecentClearBtnDisplay();
}

function ShowRecnetFileContextMenu() {
  $("#recnet_context_menu").offset({ top: 10000, left: -10000 });
  $("#recnet_context_menu").show();

  let ContextMenuWidth = $("#recnet_context_menu").width();
  let ContextMenuHeight = $("#recnet_context_menu").height();

  let DocumentWidth = $(document).width();
  let DocumentHeight = $(document).height();

  let RealX = MousePosX;
  let RealY = MousePosY;

  if (MousePosX + ContextMenuWidth + 24 > DocumentWidth)
    RealX = DocumentWidth - ContextMenuWidth - 24;
  if (MousePosY + ContextMenuHeight + 24 > DocumentHeight)
    RealY = DocumentHeight - ContextMenuHeight - 24;

  $("#recnet_context_menu").offset({ top: RealY, left: RealX });
}

/*-------RecentFile MX Message------*/
function SendMsg_GetLoginInfo() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "get_login_info";

  SendWXMessage(JSON.stringify(tSend));
}

function SendMsg_GetRecentFile() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "get_recent_projects";

  SendWXMessage(JSON.stringify(tSend));
}

function OnLoginOrRegister() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "homepage_login_or_register";

  SendWXMessage(JSON.stringify(tSend));
}

function OnGetUrl() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "rtsp_player_continue";

  SendWXMessage(JSON.stringify(tSend));
}

function OnClickModelDepot() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "homepage_modeldepot";

  SendWXMessage(JSON.stringify(tSend));
}

function OnClickNewProject() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "homepage_newproject";

  SendWXMessage(JSON.stringify(tSend));
}

function OnClickOpenProject() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "homepage_openproject";

  SendWXMessage(JSON.stringify(tSend));
}

function OnOpenRecentFile(strPath) {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "homepage_open_recentfile";
  tSend["data"] = {};
  tSend["data"]["path"] = decodeURI(strPath);

  SendWXMessage(JSON.stringify(tSend));
}

function OnDeleteRecentFile() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "homepage_delete_recentfile";
  tSend["data"] = {};
  tSend["data"]["path"] = decodeURI(RightBtnFilePath);

  SendWXMessage(JSON.stringify(tSend));

  $("#recnet_context_menu").hide();

  let AllFile = $(".FileItem");
  let nFile = AllFile.length;
  for (let p = 0; p < nFile; p++) {
    let pp = AllFile[p].getAttribute("fpath");
    if (pp == RightBtnFilePath) $(AllFile[p]).remove();
  }

  UpdateRecentClearBtnDisplay();
}

function OnDeleteAllRecentFiles() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "homepage_delete_all_recentfile";

  SendWXMessage(JSON.stringify(tSend));

  $("#FileList").html("");

  UpdateRecentClearBtnDisplay();
}

function UpdateRecentClearBtnDisplay() {
  let AllFile = $(".FileItem");
  let nFile = AllFile.length;
  if (nFile > 0) $("#RecentClearAllBtn").show();
  else $("#RecentClearAllBtn").hide();
}

function OnExploreRecentFile() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "homepage_explore_recentfile";
  tSend["data"] = {};
  tSend["data"]["path"] = decodeURI(RightBtnFilePath);

  SendWXMessage(JSON.stringify(tSend));

  $("#recnet_context_menu").hide();
}

function OnLogOut() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "homepage_logout";

  SendWXMessage(JSON.stringify(tSend));
}

function BeginDownloadNetworkPlugin() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "begin_network_plugin_download";

  SendWXMessage(JSON.stringify(tSend));
}

function OutputKey(keyCode, isCtrlDown, isShiftDown, isCmdDown) {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "get_web_shortcut";
  tSend["key_event"] = {};
  tSend["key_event"]["key"] = keyCode;
  tSend["key_event"]["ctrl"] = isCtrlDown;
  tSend["key_event"]["shift"] = isShiftDown;
  tSend["key_event"]["cmd"] = isCmdDown;

  SendWXMessage(JSON.stringify(tSend));
}

//-------------User Manual------------

function OpenWikiUrl(strUrl) {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "userguide_wiki_open";
  tSend["data"] = {};
  tSend["data"]["url"] = strUrl;

  SendWXMessage(JSON.stringify(tSend));
}

//--------------Staff Pick-------
var StaffPickSwiper = null;
function InitStaffPick() {
  if (StaffPickSwiper != null) {
    StaffPickSwiper.destroy(true, true);
    StaffPickSwiper = null;
  }

  StaffPickSwiper = new Swiper("#HotModel_Swiper.swiper", {
    slidesPerView: "auto",
    spaceBetween: 16,
    navigation: {
      nextEl: ".swiper-button-next",
      prevEl: ".swiper-button-prev",
    },
    slidesPerView: "auto",
    slidesPerGroup: 3,
    //			autoplay: {
    //				delay: 3000,
    //				stopOnLastSlide: false,
    //				disableOnInteraction: true,
    //				disableOnInteraction: false
    //			},
    //			pagination: {
    //				el: '.swiper-pagination',
    //			},
    //		    scrollbar: {
    //                el: '.swiper-scrollbar',
    //				draggable: true
    //            }
  });
}

function SendMsg_GetStaffPick() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "modelmall_model_advise_get";

  SendWXMessage(JSON.stringify(tSend));

  setTimeout("SendMsg_GetStaffPick()", 3600 * 1000 * 1);
}

function ShowStaffPick(ModelList) {
  let PickTotal = ModelList.length;
  if (PickTotal == 0) {
    $("#HotModelList").html("");
    $("#HotModelArea").hide();

    return;
  }

  let strPickHtml = "";
  for (let a = 0; a < PickTotal; a++) {
    let OnePickModel = ModelList[a];

    let ModelID = OnePickModel["design"]["id"];
    let ModelName = OnePickModel["design"]["title"];
    let ModelCover =
      OnePickModel["design"]["cover"] +
      "?image_process=resize,w_200/format,webp";

    let DesignerName = OnePickModel["design"]["designCreator"]["name"];
    let DesignerAvatar =
      OnePickModel["design"]["designCreator"]["avatar"] +
      "?image_process=resize,w_32/format,webp";

    strPickHtml +=
      '<div class="HotModelPiece swiper-slide"  onClick="OpenOneStaffPickModel(' +
      ModelID +
      ')" >' +
      '<div class="HotModel_Designer_Info"><img src="' +
      DesignerAvatar +
      '" /><span class="TextS2">' +
      DesignerName +
      "</span></div>" +
      '	<div class="HotModel_PrevBlock"><img class="HotModel_PrevImg" src="' +
      ModelCover +
      '" /></div>' +
      '	<div  class="HotModel_NameText TextS1">' +
      ModelName +
      "</div>" +
      "</div>";
  }

  $("#HotModelList").html(strPickHtml);
  InitStaffPick();
  $("#HotModelArea").show();
}

function OpenOneStaffPickModel(ModelID) {
  //alert(ModelID);
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "modelmall_model_open";
  tSend["data"] = {};
  tSend["data"]["id"] = ModelID;

  SendWXMessage(JSON.stringify(tSend));
}

//---------------Global-----------------
window.postMessage = HandleStudio;
