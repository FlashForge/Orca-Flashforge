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
  } else if (strCmd == "hidden_full_screen_icon") {
    console.log("hidden_full_screen_icon");
    // HiddenFullScreenIcon()
  } else if (strCmd == "show_full_screen_icon") {
    console.log("show_full_screen_icon");
    // ShowFullScreenIcon()
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

var flag = false;
var flag1 = false;
var flagFullScreen = false;
var currentVideoClickHandler = null;
var currentImageClickHandler = null;
var currentVideoStopPropagationHandler = null;
var currentImageStopPropagationHandler = null;

function ClearStreamTimer(timerName) {
  if (window[timerName]) {
    clearTimeout(window[timerName]);
    window[timerName] = null;
  }
}

function DestroyCurrentStream() {
  var currentVideo = document.getElementById("video");
  var currentImage = document.getElementById("videoStream");
  var currentLoader = document.getElementById("videoLoader");
  var currentPauseIcon = document.getElementById("zanting");

  ClearStreamTimer("hlsStartupTimer");
  ClearStreamTimer("hlsHardResetTimer");
  ClearStreamTimer("hlsShortRetryTimer");
  ClearStreamTimer("hlsRuntimeRecoveryTimer");
  ClearStreamTimer("flvStartupTimer");
  ClearStreamTimer("flvHardResetTimer");

  if (currentVideo && window.hlsCurrentPauseHandler) {
    currentVideo.removeEventListener("pause", window.hlsCurrentPauseHandler);
    window.hlsCurrentPauseHandler = null;
  }
  if (currentVideo && window.flvCurrentPauseHandler) {
    currentVideo.removeEventListener("pause", window.flvCurrentPauseHandler);
    window.flvCurrentPauseHandler = null;
  }
  if (currentVideo && window.flvCurrentPlayingHandler) {
    currentVideo.removeEventListener(
      "playing",
      window.flvCurrentPlayingHandler,
    );
    window.flvCurrentPlayingHandler = null;
  }

  if (window.currentHlsPlayer) {
    try {
      window.currentHlsPlayer.destroy();
    } catch (e) {
      console.warn("HLS destroy warning:", e.message);
    }
    window.currentHlsPlayer = null;
  }

  if (window.currentFlvPlayer) {
    try {
      window.currentFlvPlayer.pause();
    } catch (e) {}
    try {
      window.currentFlvPlayer.detachMediaElement();
    } catch (e) {}
    try {
      window.currentFlvPlayer.unload();
    } catch (e) {}
    try {
      window.currentFlvPlayer.destroy();
    } catch (e) {
      console.warn("FLV destroy warning:", e.message);
    }
    window.currentFlvPlayer = null;
  }

  window.flvHasFailed = false;
  streamPaused = false;

  if (currentLoader) currentLoader.style.display = "none";
  if (currentVideo) {
    try {
      currentVideo.pause();
    } catch (e) {}
    currentVideo.removeAttribute("src");
    try {
      currentVideo.load();
    } catch (e) {}
    currentVideo.style.display = "none";
    currentVideo.style.backgroundImage = "url('hei.svg')";
    currentVideo.style.backgroundSize = "cover";
    currentVideo.style.backgroundRepeat = "no-repeat";
    currentVideo.style.backgroundPosition = "center";
  }
  if (currentImage) {
    currentImage.style.display = "block";
    currentImage.src = "hei.svg";
    currentImage.style.backgroundImage = 'url("hei.svg")';
  }
  if (currentPauseIcon) currentPauseIcon.style.display = "block";
}

function SetUrlInfo(strAddress, strLanguage) {
  var handleClickFullScreen = function (event) {
    if (!event.target.closest("#full_screen_icon")) return;
    event.stopPropagation();
    event.preventDefault();
    onFullScreen();
  };

  var full_screen_icon = document.getElementById("full_screen_icon");
  var videoStream = document.getElementById("videoStream");

  if (!flagFullScreen && full_screen_icon) {
    flagFullScreen = true;
    full_screen_icon.removeEventListener("click", handleClickFullScreen, true);
    full_screen_icon.addEventListener("click", handleClickFullScreen, true);
  }

  if (!currentVideoStopPropagationHandler) {
    currentVideoStopPropagationHandler = function (e) {
      e.stopPropagation();
    };
    video.addEventListener("click", currentVideoStopPropagationHandler);
  }
  if (!currentImageStopPropagationHandler && videoStream) {
    currentImageStopPropagationHandler = function (e) {
      e.stopPropagation();
    };
    videoStream.addEventListener("click", currentImageStopPropagationHandler);
  }

  // >>> 临时测试：把 m3u8 后缀替换为 flv 测试 flv 流，测完删除此 if 块 <<<
  // if (/\.m3u8(\?|$|\/|#)/i.test(strAddress)) {
  //   strAddress = strAddress.replace(/\.m3u8/i, ".flv");
  //   console.log("[TEST] m3u8 后缀已替换为 flv:", strAddress);
  // }
  // if (/\.m3u8(\?|$|\/|#)/i.test(strAddress)) {
  //   strAddress = "http://liveplay.flashforge.com/live/test_stream_001.flv";
  //   console.log("[TEST] m3u8 后缀已替换为 flv:", strAddress);
  // }
  // <<< 临时测试结束 >>>

  // Mac 平台收到 flv 流时改用 m3u8 播放：Safari 原生支持 HLS，可绕开
  // flv.js 在 file:// 下 MSE blob:null 被 Safari 拦截的问题
  var _isMacPlatform = /macintosh|mac os x/i.test(navigator.userAgent);
  if (
    _isMacPlatform &&
    /\.flv(\?|$|\/|#)/i.test(strAddress) &&
    video.canPlayType("application/vnd.apple.mpegurl")
  ) {
    strAddress = strAddress.replace(/\.flv/i, ".m3u8");
    console.log("[Mac] flv 已改用 m3u8 播放:", strAddress);
  }

  if (window.strAddress && window.strAddress !== strAddress) {
    DestroyCurrentStream();
  }
  window.strAddress = strAddress;
  $("#url-studio").text(strAddress);
  $("#url-studio-r").text(strLanguage);
  var loader = document.getElementById("videoLoader");
  let lang = strAddress;
  console.log(lang);
  // var platform = window.navigator.platform;
  // var userAgent = window.navigator.userAgent;

  var point = document.getElementById("point");
  var point1 = document.getElementById("point1");

  const isMac = /macintosh|mac os x/i.test(navigator.userAgent);

  let zting = document.querySelector("#zanting");
  // 匹配 .m3u8 / .flv 后跟 ?、/、# 或字符串结尾，兼容带签名/参数的流地址
  function isM3u8Url(url) {
    return /\.m3u8(\?|$|\/|#)/i.test(url);
  }
  function isFlvUrl(url) {
    return /\.flv(\?|$|\/|#)/i.test(url);
  }
  if (lang) {
    // full_screen_icon.style.display = "block";
    if (isM3u8Url(lang) || isFlvUrl(lang)) {
      video.style.display = "block";
      videoStreamImg.style.display = "none";
      video.style.backgroundImage = "url('hei.svg')";
      video.style.backgroundSize = "cover";
      video.style.backgroundRepeat = "no-repeat";
      video.style.backgroundPosition = "center";

      var handleClick = function (event) {
        event.stopPropagation();
        event.preventDefault();

        // FLV 流：暂停/重播逻辑
        //  正在播放时点击 → 暂停（显示占位图）
        //  超时/失败回退后点击 → 重播
        var flvJustPaused = false;
        if (streamPaused || window.currentFlvPlayer || window.flvHasFailed) {
          var currentAddr = document.getElementById("url-studio").innerHTML;
          if (isFlvUrl(currentAddr)) {
            // 判断是"正在播放"还是"失败回退"：有播放器且非失败状态 = 正在播放
            flvJustPaused = window.currentFlvPlayer && !window.flvHasFailed;
            if (window.currentFlvPlayer) {
              try {
                window.currentFlvPlayer.pause();
                window.currentFlvPlayer.detachMediaElement();
                window.currentFlvPlayer.unload();
                window.currentFlvPlayer.destroy();
              } catch (e) {
                console.warn("FLV destroy warning:", e.message);
              }
              window.currentFlvPlayer = null;
            }
            if (window.flvHardResetTimer) {
              clearTimeout(window.flvHardResetTimer);
              window.flvHardResetTimer = null;
            }
            if (window.flvStartupTimer) {
              clearTimeout(window.flvStartupTimer);
              window.flvStartupTimer = null;
            }
            if (window.flvCurrentPauseHandler) {
              video.removeEventListener("pause", window.flvCurrentPauseHandler);
              window.flvCurrentPauseHandler = null;
            }
            if (window.flvCurrentPlayingHandler) {
              video.removeEventListener(
                "playing",
                window.flvCurrentPlayingHandler,
              );
              window.flvCurrentPlayingHandler = null;
            }
            video.src = "";
            video.style.backgroundImage = "url('hei.svg')";
            video.style.backgroundSize = "cover";
            video.style.backgroundRepeat = "no-repeat";
            video.style.backgroundPosition = "center";
            var zanting = document.getElementById("zanting");
            zanting.style.display = "block";
            window.flvHasFailed = false;
            streamPaused = false;
          }
        }

        if (!streamPaused && !flvJustPaused) {
          OnGetUrl();
          setTimeout(function () {
            var element = document.getElementById("url-studio");
            var content = element.innerHTML;
            lang = content;

            var element1 = document.getElementById("url-studio-r");
            var content1 = element1.innerHTML;
            lang1 = content1;
            if (lang === "设备离线了") {
              if (lang1 === "zh_CN") {
                point.style.display = "block";
                setTimeout(function () {
                  point.style.display = "none";
                }, 3000);
              } else if (lang1 === "en") {
                point1.style.display = "block";
                setTimeout(function () {
                  point1.style.display = "none";
                }, 3000);
              }
            } else {
              if (
                isFlvUrl(lang) &&
                typeof flvjs !== "undefined" &&
                flvjs.isSupported()
              ) {
                video.style.backgroundImage = "none";
                var zanting = document.getElementById("zanting");
                zanting.style.display = "none";

                if (!streamPaused) {
                  if (window.flvHardResetTimer) {
                    clearTimeout(window.flvHardResetTimer);
                    window.flvHardResetTimer = null;
                  }
                  if (window.flvCurrentPauseHandler) {
                    video.removeEventListener(
                      "pause",
                      window.flvCurrentPauseHandler,
                    );
                    window.flvCurrentPauseHandler = null;
                  }

                  var flvIsVideoStarting = false;
                  var flvHasFinalFallback = false;

                  function flvRollbackToPlaceholder() {
                    if (flvHasFinalFallback) return;
                    flvHasFinalFallback = true;
                    window.flvHasFailed = true;
                    loader.style.display = "none";
                    if (window.flvHardResetTimer) {
                      clearTimeout(window.flvHardResetTimer);
                      window.flvHardResetTimer = null;
                    }
                    if (window.flvStartupTimer) {
                      clearTimeout(window.flvStartupTimer);
                      window.flvStartupTimer = null;
                    }
                    if (window.flvCurrentPauseHandler) {
                      video.removeEventListener(
                        "pause",
                        window.flvCurrentPauseHandler,
                      );
                      window.flvCurrentPauseHandler = null;
                    }
                    if (window.currentFlvPlayer) {
                      try {
                        window.currentFlvPlayer.pause();
                        window.currentFlvPlayer.detachMediaElement();
                        window.currentFlvPlayer.unload();
                        window.currentFlvPlayer.destroy();
                      } catch (e) {
                        console.warn("FLV destroy warning:", e.message);
                      }
                      window.currentFlvPlayer = null;
                    }
                    video.src = "";
                    video.style.backgroundImage = "url('hei.svg')";
                    video.style.backgroundSize = "cover";
                    video.style.backgroundRepeat = "no-repeat";
                    video.style.backgroundPosition = "center";
                    var zanting = document.getElementById("zanting");
                    zanting.style.display = "block";
                    streamPaused = false;
                    if (lang1 === "zh_CN") {
                      point.style.display = "block";
                      setTimeout(function () {
                        point.style.display = "none";
                      }, 3000);
                    } else if (lang1 === "en") {
                      point1.style.display = "block";
                      setTimeout(function () {
                        point1.style.display = "none";
                      }, 3000);
                    }
                  }

                  // 安全销毁 FLV 播放器，避免 destroy 后异步 pump 访问已移除的 SourceBuffer 报错
                  function safeDestroyFlvPlayer() {
                    var p = window.currentFlvPlayer;
                    if (!p) return;
                    window.currentFlvPlayer = null;
                    try {
                      p.pause();
                    } catch (e) {}
                    try {
                      p.detachMediaElement();
                    } catch (e) {}
                    try {
                      p.unload();
                    } catch (e) {}
                    try {
                      p.destroy();
                    } catch (e) {
                      console.warn("FLV destroy warning:", e.message);
                    }
                  }

                  function startFlvPlayback() {
                    if (flvHasFinalFallback) return;
                    if (window.currentFlvPlayer) {
                      safeDestroyFlvPlayer();
                    }
                    var flvPlayer = flvjs.createPlayer({
                      type: "flv",
                      url: lang,
                      isLive: true,
                    });
                    window.currentFlvPlayer = flvPlayer;
                    flvPlayer.attachMediaElement(video);
                    flvPlayer.load();

                    flvPlayer.on(flvjs.Events.ERROR, function () {
                      if (flvHasFinalFallback) return;
                      flvRollbackToPlaceholder();
                    });

                    video.play().catch(function (e) {
                      console.error("FLV播放失败:", e);
                    });
                  }

                  if (video.readyState < 2) {
                    loader.style.display = "block";
                    startFlvPlayback();
                  }

                  function onFlvPlaying(event) {
                    event.stopPropagation();
                    flvIsVideoStarting = true;
                    loader.style.display = "none";
                  }
                  function onFlvPause(event) {
                    event.stopPropagation();
                    loader.style.display = "none";
                  }

                  video.addEventListener("playing", onFlvPlaying, {
                    once: true,
                  });
                  window.flvCurrentPlayingHandler = onFlvPlaying;
                  window.flvCurrentPauseHandler = onFlvPause;
                  video.addEventListener("pause", onFlvPause);

                  window.flvStartupTimer = setTimeout(function () {
                    window.flvStartupTimer = null;
                    if (!flvIsVideoStarting) {
                      flvRollbackToPlaceholder();
                    }
                  }, 30000);

                  window.flvHardResetTimer = setTimeout(function () {
                    window.flvHardResetTimer = null;
                    if (flvHasFinalFallback) return;
                    if (window.flvCurrentPauseHandler) {
                      video.removeEventListener(
                        "pause",
                        window.flvCurrentPauseHandler,
                      );
                      window.flvCurrentPauseHandler = null;
                    }
                    if (window.currentFlvPlayer) {
                      try {
                        window.currentFlvPlayer.pause();
                        window.currentFlvPlayer.detachMediaElement();
                        window.currentFlvPlayer.unload();
                        window.currentFlvPlayer.destroy();
                      } catch (e) {
                        console.warn("FLV destroy warning:", e.message);
                      }
                      window.currentFlvPlayer = null;
                    }
                    video.src = "";
                    video.style.backgroundImage = "url('hei.svg')";
                    video.style.backgroundSize = "cover";
                    video.style.backgroundRepeat = "no-repeat";
                    video.style.backgroundPosition = "center";
                    var zanting = document.getElementById("zanting");
                    zanting.style.display = "block";
                    streamPaused = false;
                  }, 300000);
                  streamPaused = true;
                }
              } else if (isMac) {
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
                      loader.style.display = "block";
                      // 尝试播放视频
                      video.play().catch(function (e) {
                        console.error("播放失败:", e);
                      });
                    }

                    // 设置一个标志来跟踪视频是否开始播放
                    var isVideoStarting = false;

                    function hideLoader(event) {
                      event.stopPropagation();
                      isVideoStarting = true;
                      loader.style.display = "none";
                    }

                    // 当视频开始播放时，隐藏加载动画
                    video.addEventListener("playing", hideLoader);

                    // 视频暂停时，不显示加载动画
                    video.addEventListener("pause", hideLoader);

                    // 视频发生错误时显示加载动画
                    video.addEventListener("error", hideLoader);

                    // 设置一个超时时间，在30秒后检查视频是否开始播放
                    setTimeout(function () {
                      if (!isVideoStarting) {
                        loader.style.display = "none";
                        video.src = "";
                        video.style.backgroundImage = "url('hei.svg')";
                        video.style.backgroundSize = "cover";
                        video.style.backgroundRepeat = "no-repeat";
                        video.style.backgroundPosition = "center";
                        var zanting = document.getElementById("zanting");
                        zanting.style.display = "block";
                        streamPaused = false;
                        if (lang1 === "zh_CN") {
                          point.style.display = "block";
                          setTimeout(function () {
                            point.style.display = "none";
                          }, 3000);
                        } else if (lang1 === "en") {
                          point1.style.display = "block";
                          setTimeout(function () {
                            point1.style.display = "none";
                          }, 3000);
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

                  if (!streamPaused) {
                    // 清除上一次 session 残留的 5 分钟定时器，防止跨 session 干扰
                    if (window.hlsHardResetTimer) {
                      clearTimeout(window.hlsHardResetTimer);
                      window.hlsHardResetTimer = null;
                    }
                    // 清除上一次 session 残留的 pause 监听器
                    if (window.hlsCurrentPauseHandler) {
                      video.removeEventListener(
                        "pause",
                        window.hlsCurrentPauseHandler,
                      );
                      window.hlsCurrentPauseHandler = null;
                    }

                    var maxShortRetry = 5;
                    var shortRetryDelay = 4000;
                    var shortRetryCount = 0;
                    var maxRuntimeRetry = 6;
                    var runtimeRetryDelay = 3000;
                    var runtimeRetryCount = 0;
                    var runtimeRetryPending = false;
                    var isVideoStarting = false;
                    var hasFinalFallback = false;

                    function rollbackToPlaceholder() {
                      if (hasFinalFallback) return;
                      hasFinalFallback = true;
                      loader.style.display = "none";
                      if (window.hlsHardResetTimer) {
                        clearTimeout(window.hlsHardResetTimer);
                        window.hlsHardResetTimer = null;
                      }
                      ClearStreamTimer("hlsStartupTimer");
                      ClearStreamTimer("hlsShortRetryTimer");
                      ClearStreamTimer("hlsRuntimeRecoveryTimer");
                      if (window.hlsCurrentPauseHandler) {
                        video.removeEventListener(
                          "pause",
                          window.hlsCurrentPauseHandler,
                        );
                        window.hlsCurrentPauseHandler = null;
                      }
                      if (window.currentHlsPlayer) {
                        window.currentHlsPlayer.destroy();
                        window.currentHlsPlayer = null;
                      }
                      video.src = "";
                      video.style.backgroundImage = "url('hei.svg')";
                      video.style.backgroundSize = "cover";
                      video.style.backgroundRepeat = "no-repeat";
                      video.style.backgroundPosition = "center";
                      var zanting = document.getElementById("zanting");
                      zanting.style.display = "block";
                      streamPaused = false;
                      if (lang1 === "zh_CN") {
                        point.style.display = "block";
                        setTimeout(function () {
                          point.style.display = "none";
                        }, 3000);
                      } else if (lang1 === "en") {
                        point1.style.display = "block";
                        setTimeout(function () {
                          point1.style.display = "none";
                        }, 3000);
                      }
                    }

                    function scheduleRuntimeRecovery(hls) {
                      if (hasFinalFallback || runtimeRetryPending) return;
                      if (runtimeRetryCount >= maxRuntimeRetry) {
                        rollbackToPlaceholder();
                        return;
                      }

                      runtimeRetryPending = true;
                      runtimeRetryCount++;
                      loader.style.display = "block";

                      window.hlsRuntimeRecoveryTimer = setTimeout(function () {
                        window.hlsRuntimeRecoveryTimer = null;
                        runtimeRetryPending = false;
                        if (hasFinalFallback) return;

                        if (!window.currentHlsPlayer) {
                          startHlsPlayback();
                          return;
                        }

                        try {
                          var activeHls = window.currentHlsPlayer;
                          if (!activeHls) {
                            startHlsPlayback();
                            return;
                          }
                          activeHls.startLoad(-1);
                          video.play().catch(function (e) {
                            console.error("播放失败:", e);
                          });
                        } catch (e) {
                          startHlsPlayback();
                        }
                      }, runtimeRetryDelay);
                    }

                    function startHlsPlayback() {
                      if (hasFinalFallback) return;
                      if (window.currentHlsPlayer) {
                        window.currentHlsPlayer.destroy();
                        window.currentHlsPlayer = null;
                      }
                      var hls = new Hls();
                      window.currentHlsPlayer = hls;
                      hls.loadSource(lang);
                      hls.attachMedia(video);

                      hls.on(Hls.Events.ERROR, function (eventName, data) {
                        if (!data || hasFinalFallback) return;

                        var isNetworkError =
                          data.type === Hls.ErrorTypes.NETWORK_ERROR;
                        var isMediaError =
                          data.type === Hls.ErrorTypes.MEDIA_ERROR;

                        // 起播前错误：沿用短期重试
                        if (!isVideoStarting && data.fatal) {
                          if (shortRetryCount < maxShortRetry) {
                            shortRetryCount++;
                            ClearStreamTimer("hlsShortRetryTimer");
                            window.hlsShortRetryTimer = setTimeout(function () {
                              window.hlsShortRetryTimer = null;
                              startHlsPlayback();
                            }, shortRetryDelay);
                            return;
                          }
                          rollbackToPlaceholder();
                          return;
                        }

                        // 已起播后的网络抖动/丢流：执行运行期恢复
                        if (isVideoStarting && isNetworkError) {
                          scheduleRuntimeRecovery(hls);
                          return;
                        }

                        // 媒体解码类错误尝试恢复
                        if (isVideoStarting && isMediaError && data.fatal) {
                          try {
                            hls.recoverMediaError();
                          } catch (e) {
                            rollbackToPlaceholder();
                          }
                          return;
                        }

                        if (data.fatal) rollbackToPlaceholder();
                      });

                      video.play().catch(function (e) {
                        console.error("播放失败:", e);
                      });
                    }

                    // 当视频被点击，检查其播放状态
                    if (video.readyState < 2) {
                      // 如果视频未加载足够的数据，则显示加载动画
                      loader.style.display = "block";
                      startHlsPlayback();
                    } else {
                      // 已有数据（如暂停后恢复），直接恢复播放
                      if (window.currentHlsPlayer) {
                        try { window.currentHlsPlayer.startLoad(-1); } catch (e) {}
                      }
                      video.play().catch(function (e) {
                        console.error("播放失败:", e);
                      });
                    }

                    function onPlaying(event) {
                      event.stopPropagation();
                      isVideoStarting = true;
                      runtimeRetryCount = 0;
                      runtimeRetryPending = false;
                      loader.style.display = "none";
                    }

                    function onPause(event) {
                      event.stopPropagation();
                      loader.style.display = "none";
                    }

                    // 当视频开始播放时，隐藏加载动画
                    video.addEventListener("playing", onPlaying, {
                      once: true,
                    });
                    // 视频暂停时，不显示加载动画（用全局引用，方便 rollback 时清除）
                    window.hlsCurrentPauseHandler = onPause;
                    video.addEventListener("pause", onPause);

                    // 设置一个超时时间，在30秒后检查视频是否开始播放
                    window.hlsStartupTimer = setTimeout(function () {
                      window.hlsStartupTimer = null;
                      if (!isVideoStarting) {
                        rollbackToPlaceholder();
                      }
                    }, 30000); // 30秒超时时间

                    // 5分钟强制停流，用全局引用防止跨 session 干扰
                    window.hlsHardResetTimer = setTimeout(function () {
                      window.hlsHardResetTimer = null;
                      if (hasFinalFallback) return; // rollback 已经处理过，无需重复
                      if (window.hlsCurrentPauseHandler) {
                        video.removeEventListener(
                          "pause",
                          window.hlsCurrentPauseHandler,
                        );
                        window.hlsCurrentPauseHandler = null;
                      }
                      if (window.currentHlsPlayer) {
                        window.currentHlsPlayer.destroy();
                        window.currentHlsPlayer = null;
                      }
                      video.src = "";
                      video.style.backgroundImage = "url('hei.svg')";
                      video.style.backgroundSize = "cover";
                      video.style.backgroundRepeat = "no-repeat";
                      video.style.backgroundPosition = "center";
                      var zanting = document.getElementById("zanting");
                      zanting.style.display = "block";
                      streamPaused = false;
                    }, 300000); // 5分钟后强制停流
                    streamPaused = true;
                  }
                }
              }
            }
          }, 10);
        } else {
          // 暂停视频：停在最后一帧，不覆盖背景图
          if (window.currentHlsPlayer) {
            try { window.currentHlsPlayer.stopLoad(); } catch (e) {}
          }
          video.pause();
          var zanting = document.getElementById("zanting");
          zanting.style.display = "block";
          streamPaused = false;
        }
      };

      if (currentImageClickHandler) {
        for (let i = 0; i < imgs.length; i++) {
          imgs[i].removeEventListener("click", currentImageClickHandler);
        }
        currentImageClickHandler = null;
        flag1 = false;
      }
      if (currentVideoClickHandler) {
        zting.removeEventListener("click", currentVideoClickHandler);
        video.removeEventListener("click", currentVideoClickHandler);
      }
      currentVideoClickHandler = handleClick;
      zting.addEventListener("click", currentVideoClickHandler);
      video.addEventListener("click", currentVideoClickHandler);
      flag = true;
    } else {
      // flag = false;
      // alert(flag)
      // video.style.display = 'none'
      // videoStreamImg.src = 'hei.svg'
      // videoStreamImg.style.backgroundImage = 'url("hei.svg")'
      var handleClicktr = function (event) {
        event.stopPropagation();
        event.preventDefault();
        if (!streamPaused) {
          OnGetUrl();
          setTimeout(function () {
            var element = document.getElementById("url-studio");
            var content = element.innerHTML;
            lang = content;

            var element1 = document.getElementById("url-studio-r");
            var content1 = element1.innerHTML;
            lang1 = content1;
            if (lang === "设备离线了") {
              if (lang1 === "zh_CN") {
                point.style.display = "block";
                setTimeout(function () {
                  point.style.display = "none";
                }, 3000);
              } else if (lang1 === "en") {
                point1.style.display = "block";
                setTimeout(function () {
                  point1.style.display = "none";
                }, 3000);
              }
            } else {
              loader.style.display = "block";
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

              videoStream.addEventListener(
                "load",
                function (event) {
                  event.stopPropagation();
                  event.preventDefault();
                  isVideoStarting = true;
                  // 隐藏加载动画
                  videoLoader.style.display = "none";
                },
                { once: true },
              ); // 使用 { once: true } 参数，确保事件只触发一次

              videoStream.addEventListener("error", function (event) {
                event.stopPropagation();
                event.preventDefault();
                isVideoStarting = true;
                // 隐藏加载动画
                videoLoader.style.display = "none";
              });

              // 设置一个超时时间，在30秒后检查视频是否开始播放
              setTimeout(function () {
                if (!isVideoStarting) {
                  loader.style.display = "none";
                  videoStreamImg.src = "";
                  videoStreamImg.src = "hei.svg";
                  var zanting = document.getElementById("zanting");
                  zanting.style.display = "block";
                  videoStreamImg.style.backgroundImage = 'url("hei.svg")';
                  streamPaused = false;
                  if (lang1 === "zh_CN") {
                    point.style.display = "block";
                    setTimeout(function () {
                      point.style.display = "none";
                    }, 3000);
                  } else if (lang1 === "en") {
                    point1.style.display = "block";
                    setTimeout(function () {
                      point1.style.display = "none";
                    }, 3000);
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
      };

      if (currentVideoClickHandler) {
        zting.removeEventListener("click", currentVideoClickHandler);
        video.removeEventListener("click", currentVideoClickHandler);
        currentVideoClickHandler = null;
        flag = false;
      }
      if (currentImageClickHandler) {
        for (let i = 0; i < imgs.length; i++) {
          imgs[i].removeEventListener("click", currentImageClickHandler);
        }
      }
      currentImageClickHandler = handleClicktr;
      for (let i = 0; i < imgs.length; i++) {
        imgs[i].addEventListener("click", currentImageClickHandler);
      }
      flag1 = true;
    }
  } else {
    video.style.display = "none";
    videoStreamImg.src = "hei.svg";
    videoStreamImg.style.backgroundImage = 'url("hei.svg")';
    for (let i = 0; i < imgs.length; i++) {
      imgs[i].addEventListener("click", function (event) {
        event.stopPropagation();
        event.preventDefault();
        alert("无法获取视频流路径");
      });
    }
  }
}

function HiddenFullScreenIcon() {
  var full_screen_icon = document.getElementById("full_screen_icon");
  if (full_screen_icon) {
    full_screen_icon.style.display = "none";
  }
}
function ShowFullScreenIcon() {
  var full_screen_icon = document.getElementById("full_screen_icon");
  if (full_screen_icon) {
    full_screen_icon.style.display = "block";
  }
}

function SetClose() {
  DestroyCurrentStream();
  window.strAddress = "";
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

function onFullScreen() {
  var tSend = {};
  tSend["sequence_id"] = Math.round(new Date() / 1000);
  tSend["command"] = "full_screen";
  SendWXMessage(JSON.stringify(tSend));
}

//---------------Global-----------------
window.postMessage = HandleStudio;
