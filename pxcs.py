import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import threading
from openai import OpenAI
import time
import queue
import json
from datetime import datetime, timedelta
from concurrent.futures import ThreadPoolExecutor
import random
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import os
import re
import requests
from bs4 import BeautifulSoup
import urllib.parse


class SavingsPlannerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("💰 貔貅计划助手 ")
        self.root.geometry("1000x850")
        self.root.configure(bg='#2C3E50')

        # 初始化OpenAI客户端
        self.client = OpenAI(
            api_key="sk-683e180442944fddb15fc935790bcfdd",
            base_url="https://api.deepseek.com"
        )

        # 初始化线程池
        self.thread_pool = ThreadPoolExecutor(max_workers=5)

        # 攒钱数据
        self.savings_data = {
            "target_amount": 0.0,
            "current_amount": 0.0,
            "monthly_income": 0.0,
            "monthly_expenses": 0.0,
            "target_item": "",
            "item_price": 0.0,
            "price_history": [],
            "savings_history": [],
            "last_income_update": None,
            "amount_history": [],  # 新增：记录每刻金额变化
            "targets": []  # 新增：多个攒钱目标
        }

        # 当前选中的目标索引
        self.current_target_index = -1

        # 检查依赖库
        self.check_dependencies()

        # 设置样式
        self.setup_styles()

        # 创建界面
        self.create_widgets()

        # 数据持久化文件路径
        self.data_file = os.path.join(os.path.dirname(__file__), "pxcs_data.json")

        # 加载上次保存的数据并更新界面
        try:
            self.load_savings_data()
            self.refresh_targets_list()
            self.update_target_combobox()
            self.update_current_target_display()
            # 同步当前金额到输入框
            self.current_entry.delete(0, tk.END)
            self.current_entry.insert(0, f"{self.savings_data['current_amount']:.2f}")
        except Exception:
            pass

        # 活跃请求跟踪
        self.active_requests = set()

        # 启动价格监控和金额记录
        self.start_price_monitoring()
        self.start_amount_recording()

    def check_dependencies(self):
        """检查必要的依赖库"""
        missing_libs = []
        try:
            import openpyxl
        except ImportError:
            missing_libs.append("openpyxl")

        try:
            import pandas as pd
        except ImportError:
            missing_libs.append("pandas")

        try:
            import matplotlib
        except ImportError:
            missing_libs.append("matplotlib")

        try:
            import bs4
        except ImportError:
            missing_libs.append("beautifulsoup4")

        if missing_libs:
            messagebox.showwarning(
                "缺少依赖库",
                f"以下库未安装：{', '.join(missing_libs)}\n"
                f"请运行：pip install {' '.join(missing_libs)}"
            )

    def setup_styles(self):
        style = ttk.Style()
        style.theme_use('clam')

        # 配置样式
        style.configure('TFrame', background='#2C3E50')
        style.configure('TLabel', background='#2C3E50', foreground='white')
        style.configure('TLabelframe', background='#34495E', foreground='white')
        style.configure('TLabelframe.Label', background='#34495E', foreground='#1ABC9C')

        style.configure('Primary.TButton',
                        background='#1ABC9C',
                        foreground='white',
                        font=('微软雅黑', 10, 'bold'),
                        borderwidth=0)
        style.configure('Secondary.TButton',
                        background='#3498DB',
                        foreground='white',
                        font=('微软雅黑', 10),
                        borderwidth=0)
        style.configure('Success.TButton',
                        background='#2ECC71',
                        foreground='white',
                        font=('微软雅黑', 10),
                        borderwidth=0)
        style.configure('Warning.TButton',
                        background='#E74C3C',
                        foreground='white',
                        font=('微软雅黑', 10),
                        borderwidth=0)
        style.configure('Info.TButton',
                        background='#9B59B6',
                        foreground='white',
                        font=('微软雅黑', 10),
                        borderwidth=0)

        style.map('Primary.TButton',
                  background=[('active', '#16A085'), ('pressed', '#149174')])
        style.map('Secondary.TButton',
                  background=[('active', '#2980B9'), ('pressed', '#2471A3')])
        style.map('Success.TButton',
                  background=[('active', '#27AE60'), ('pressed', '#229954')])
        style.map('Warning.TButton',
                  background=[('active', '#CB4335'), ('pressed', '#B03A2E')])
        style.map('Info.TButton',
                  background=[('active', '#8E44AD'), ('pressed', '#7D3C98')])

    def create_widgets(self):
        # 主框架
        main_frame = ttk.Frame(self.root, padding="15")
        main_frame.pack(fill=tk.BOTH, expand=True)

        # 标题区域
        title_frame = ttk.Frame(main_frame)
        title_frame.pack(fill=tk.X, pady=(0, 15))

        title_label = tk.Label(title_frame,
                               text="💰 貔貅计划助手 - 智能攒钱规划",
                               font=('微软雅黑', 18, 'bold'),
                               fg='#1ABC9C',
                               bg='#2C3E50')
        title_label.pack(side=tk.LEFT)

        # 导出按钮
        export_button = ttk.Button(title_frame,
                                   text="📊 导出Excel",
                                   command=self.export_to_excel,
                                   style='Secondary.TButton',
                                   width=12)
        export_button.pack(side=tk.RIGHT, padx=(10, 0))

        # 图表按钮
        chart_button = ttk.Button(title_frame,
                                  text="📈 查看图表",
                                  command=self.show_charts,
                                  style='Secondary.TButton',
                                  width=12)
        chart_button.pack(side=tk.RIGHT, padx=(10, 0))

        # 目标管理按钮
        target_button = ttk.Button(title_frame,
                                   text="🎯 目标管理",
                                   command=self.show_target_manager,
                                   style='Info.TButton',
                                   width=12)
        target_button.pack(side=tk.RIGHT, padx=(10, 0))

        # 创建分页导航
        self.create_notebook(main_frame)

        # 状态栏
        self.status_var = tk.StringVar()
        self.status_var.set("🟢 系统就绪")
        status_bar = tk.Label(main_frame,
                              textvariable=self.status_var,
                              relief=tk.SUNKEN,
                              anchor=tk.W,
                              font=('微软雅黑', 9),
                              fg='#BDC3C7',
                              bg='#34495E')
        status_bar.pack(fill=tk.X, side=tk.BOTTOM, pady=(8, 0))

        # 欢迎消息
        welcome_msg = """🎉 欢迎使用貔貅计划助手！💰

🌟 功能特点：
• 🤖 AI自动查询物品价格并设置目标
• 🚗 智能爬虫（汽车之家/慢慢买）
• 💵 工资到账自动计算净收入
• 📊 实时跟踪攒钱进度和时间估算
• 📈 价格变化监控和图表分析
• 💾 Excel数据导出功能
• 🎯 多目标管理和个性化AI攒钱建议

🚀 使用方法：
1. 在"目标管理"中添加多个攒钱目标
2. 选择要跟踪的目标
3. 设置月收入和月支出
4. 点击"工资到账"自动计算净收入
5. 使用"导出Excel"保存数据"""
        self.add_message("系统", welcome_msg, "system")

    def create_notebook(self, parent):
        """创建分页导航"""
        self.notebook = ttk.Notebook(parent)
        self.notebook.pack(fill=tk.BOTH, expand=True, pady=(0, 10))

        # 创建主要功能页
        self.main_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.main_frame, text="🏠 主要功能")

        # 创建目标管理页
        self.targets_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.targets_frame, text="🎯 目标管理")

        # 初始化各页面
        self.create_main_page()
        self.create_targets_page()

    def create_main_page(self):
        """创建主要功能页面（左右分栏，更饱满合理）"""
        # 顶层左右分栏
        self.main_paned = ttk.Panedwindow(self.main_frame, orient=tk.HORIZONTAL)
        self.main_paned.pack(fill=tk.BOTH, expand=True)

        self.left_panel = ttk.Frame(self.main_paned)
        self.right_panel = ttk.Frame(self.main_paned)
        self.main_paned.add(self.left_panel, weight=3)
        self.main_paned.add(self.right_panel, weight=1)

        # 绑定窗口大小变化，限制右侧最大宽度，避免遮挡
        try:
            self.main_frame.bind('<Configure>', self.on_main_resize)
        except Exception:
            pass

        # 初始设置为折叠，确保主功能不被遮挡
        self.root.update_idletasks()
        try:
            total_w = self.root.winfo_width()
            self.main_paned.sashpos(0, int(total_w * 0.96))  # 右侧仅约4%
        except Exception:
            pass
        self.chat_collapsed = True

        # 为后续局部变量兼容
        left_panel = self.left_panel
        right_panel = self.right_panel

        # --- 左侧：目标、资金、操作、进度 ---
        # 目标选择区域
        target_select_frame = ttk.LabelFrame(left_panel, text="🎯 当前目标", padding="10")
        target_select_frame.pack(fill=tk.X, pady=(0, 12))

        target_select_row = ttk.Frame(target_select_frame)
        target_select_row.pack(fill=tk.X, pady=5)

        ttk.Label(target_select_row, text="选择目标:", font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 10))

        self.target_combobox = ttk.Combobox(target_select_row,
                                            values=[],
                                            state="readonly",
                                            width=30,
                                            font=('微软雅黑', 10))
        self.target_combobox.pack(side=tk.LEFT, padx=(0, 10))
        self.target_combobox.bind('<<ComboboxSelected>>', self.on_target_selected)

        ttk.Button(target_select_row,
                   text="刷新目标",
                   command=self.refresh_targets,
                   style='Secondary.TButton').pack(side=tk.LEFT)

        self.current_target_label = ttk.Label(target_select_frame,
                                              text="暂无选中目标",
                                              font=('微软雅黑', 10),
                                              foreground='#F39C12')
        self.current_target_label.pack(pady=5)

        # 当前已攒输入
        current_amount_row = ttk.Frame(target_select_frame)
        current_amount_row.pack(fill=tk.X, pady=5)
        ttk.Label(current_amount_row, text="当前已攒:", width=10, font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 8))
        self.current_entry = ttk.Entry(current_amount_row, width=15, font=('微软雅黑', 10))
        self.current_entry.pack(side=tk.LEFT)

        # 输入区域
        input_frame = ttk.LabelFrame(left_panel, text="💰 资金设置", padding="12")
        input_frame.pack(fill=tk.X, pady=(0, 12))

        # 第一行输入
        row1_frame = ttk.Frame(input_frame)
        row1_frame.pack(fill=tk.X, pady=6)

        ttk.Label(row1_frame, text="目标物品:", width=10, font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 8))
        self.item_entry = ttk.Entry(row1_frame, width=22, font=('微软雅黑', 10))
        self.item_entry.pack(side=tk.LEFT, padx=(0, 12))

        # 添加查询方式下拉列表
        ttk.Label(row1_frame, text="查询方式:", width=8, font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 5))
        self.search_method = ttk.Combobox(row1_frame,
                                          values=["AI查询", "汽车之家", "慢慢买"],
                                          state="readonly",
                                          width=8,
                                          font=('微软雅黑', 10))
        self.search_method.pack(side=tk.LEFT, padx=(0, 10))
        self.search_method.set("AI查询")

        ttk.Button(row1_frame,
                   text="查询价格",
                   command=self.query_price,
                   style='Secondary.TButton',
                   width=10).pack(side=tk.LEFT, padx=(0, 15))

        # 当前已攒输入已移动到“当前目标”分组中
        # 第二行输入
        row2_frame = ttk.Frame(input_frame)
        row2_frame.pack(fill=tk.X, pady=6)

        ttk.Label(row2_frame, text="月收入:", width=10, font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 8))
        self.income_entry = ttk.Entry(row2_frame, width=15, font=('微软雅黑', 10))
        self.income_entry.pack(side=tk.LEFT, padx=(0, 15))

        ttk.Label(row2_frame, text="月支出:", width=10, font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 8))
        self.expenses_entry = ttk.Entry(row2_frame, width=15, font=('微软雅黑', 10))
        self.expenses_entry.pack(side=tk.LEFT, padx=(0, 15))

        ttk.Button(row2_frame,
                   text="💵 收入到账",
                   command=self.add_income_to_savings,
                   style='Success.TButton',
                   width=12).pack(side=tk.LEFT)

        # 第三行显示信息
        row3_frame = ttk.Frame(input_frame)
        row3_frame.pack(fill=tk.X, pady=6)

        self.price_info_label = ttk.Label(row3_frame,
                                          text="请输入目标物品名称，选择查询方式并点击'查询价格'",
                                          foreground='#BDC3C7',
                                          font=('微软雅黑', 9))
        self.price_info_label.pack(side=tk.LEFT)

        self.monthly_savings_label = ttk.Label(row3_frame,
                                               text="",
                                               foreground='#1ABC9C',
                                               font=('微软雅黑', 10, 'bold'))
        self.monthly_savings_label.pack(side=tk.RIGHT)

        # 金额操作区域
        operation_frame = ttk.LabelFrame(left_panel, text="💳 金额操作", padding="12")
        operation_frame.pack(fill=tk.X, pady=(0, 12))

        op_row_frame = ttk.Frame(operation_frame)
        op_row_frame.pack(fill=tk.X, pady=6)

        ttk.Label(op_row_frame, text="操作金额:", font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 8))
        self.amount_entry = ttk.Entry(op_row_frame, width=15, font=('微软雅黑', 10))
        self.amount_entry.pack(side=tk.LEFT, padx=(0, 12))

        ttk.Button(op_row_frame, text="➕ 增加金额", command=self.add_amount, style='Success.TButton').pack(side=tk.LEFT,
                                                                                                           padx=6)
        ttk.Button(op_row_frame, text="➖ 减少金额", command=self.subtract_amount, style='Warning.TButton').pack(
            side=tk.LEFT, padx=6)
        ttk.Button(op_row_frame, text="✅ 确认设置", command=self.set_target, style='Primary.TButton').pack(side=tk.LEFT,
                                                                                                           padx=6)

        # 进度显示区域
        progress_frame = ttk.LabelFrame(left_panel, text="📊 攒钱进度", padding="12")
        progress_frame.pack(fill=tk.X, pady=(0, 12))

        # 进度条
        self.progress_var = tk.DoubleVar()
        self.progress_bar = ttk.Progressbar(progress_frame, variable=self.progress_var, maximum=100,
                                            style='custom.Horizontal.TProgressbar')
        self.progress_bar.pack(fill=tk.X, pady=8)

        # 自定义进度条样式
        style = ttk.Style()
        style.configure('custom.Horizontal.TProgressbar',
                        background='#1ABC9C',
                        troughcolor='#34495E')

        # 进度信息
        self.progress_label = ttk.Label(progress_frame, text="请先设置攒钱目标",
                                        font=('微软雅黑', 11, 'bold'), foreground='#ECF0F1')
        self.progress_label.pack(pady=4)

        # 时间估算
        self.time_label = ttk.Label(progress_frame, text="",
                                    font=('微软雅黑', 9), foreground='#BDC3C7')
        self.time_label.pack(pady=2)

        # AI思考进度条
        self.ai_progress_var = tk.DoubleVar()
        self.ai_progress_bar = ttk.Progressbar(progress_frame, variable=self.ai_progress_var, maximum=100,
                                               mode='indeterminate', style='ai.Horizontal.TProgressbar')
        style.configure('ai.Horizontal.TProgressbar',
                        background='#3498DB',
                        troughcolor='#34495E')

        # --- 右侧：对话（上） + AI建议（下） + 操作按钮 ---
        chat_paned = ttk.Panedwindow(right_panel, orient=tk.VERTICAL)
        chat_paned.pack(fill=tk.BOTH, expand=True, pady=(0, 12))

        # 上：聊天区域
        top_chat_frame = ttk.LabelFrame(chat_paned, text="💬 对话记录", padding="12")
        chat_paned.add(top_chat_frame, weight=3)

        self.chat_display = scrolledtext.ScrolledText(
            top_chat_frame,
            wrap=tk.WORD,
            font=('微软雅黑', 10),
            bg='#2C3E50',
            fg='#ECF0F1',
            padx=12,
            pady=12,
            state=tk.DISABLED,
            relief='flat',
            bd=0
        )
        self.chat_display.pack(fill=tk.BOTH, expand=True)

        # 下：AI建议区域
        bottom_advice_frame = ttk.LabelFrame(chat_paned, text="🤖 AI建议", padding="12")
        chat_paned.add(bottom_advice_frame, weight=2)

        self.advice_text = scrolledtext.ScrolledText(
            bottom_advice_frame,
            wrap=tk.WORD,
            font=('微软雅黑', 10),
            bg='#2C3E50',
            fg='#ECF0F1',
            padx=12,
            pady=12,
            state=tk.DISABLED,
            relief='flat',
            bd=0
        )
        self.advice_text.pack(fill=tk.BOTH, expand=True)

        # 右侧底部：操作按钮横排
        button_bar = ttk.Frame(right_panel)
        button_bar.pack(fill=tk.X, pady=(0, 0))

        # 折叠/展开建议区按钮，避免遮挡主要功能
        self.toggle_chat_btn = ttk.Button(
            button_bar,
            text="隐藏建议区",
            command=self.toggle_chat_panel,
            style='Secondary.TButton',
            width=12
        )
        self.toggle_chat_btn.pack(side=tk.LEFT, padx=6)
        # 启动时若为折叠状态，按钮文案改为“显示建议区”
        try:
            if getattr(self, 'chat_collapsed', False):
                self.toggle_chat_btn.config(text="显示建议区")
        except Exception:
            pass

        self.cancel_button = ttk.Button(
            button_bar,
            text="取消请求",
            command=self.cancel_requests,
            style='Warning.TButton',
            width=10
        )
        self.cancel_button.pack(side=tk.RIGHT, padx=6)
        self.cancel_button.config(state=tk.DISABLED)

        self.clear_button = ttk.Button(
            button_bar,
            text="清空对话",
            command=self.clear_chat,
            style='Secondary.TButton',
            width=10
        )
        self.clear_button.pack(side=tk.RIGHT, padx=6)

        self.advice_button = ttk.Button(
            button_bar,
            text="获取攒钱建议",
            command=self.get_savings_advice,
            style='Primary.TButton',
            width=12
        )
        self.advice_button.pack(side=tk.RIGHT, padx=6)

        # 配置聊天区域标签样式
        self.chat_display.tag_config("user_tag", foreground="#3498DB", font=('微软雅黑', 9, 'bold'))
        self.chat_display.tag_config("user_msg", foreground="#ECF0F1")
        self.chat_display.tag_config("assistant_tag", foreground="#1ABC9C", font=('微软雅黑', 9, 'bold'))
        self.chat_display.tag_config("assistant_msg", foreground="#ECF0F1")
        self.chat_display.tag_config("system_tag", foreground="#F39C12", font=('微软雅黑', 9, 'italic'))
        self.chat_display.tag_config("system_msg", foreground="#BDC3C7")

        # 绑定收入支出输入框的变化事件
        self.income_entry.bind('<KeyRelease>', self.update_monthly_savings_display)
        self.expenses_entry.bind('<KeyRelease>', self.update_monthly_savings_display)

    def toggle_chat_panel(self):
        """折叠/展开右侧建议与对话区域"""
        try:
            if not hasattr(self, 'main_paned') or not hasattr(self, 'right_panel'):
                return
            # 切换折叠状态
            self.chat_collapsed = not getattr(self, 'chat_collapsed', False)
            if self.chat_collapsed:
                # 将分栏拖到右侧，几乎隐藏右侧
                self.root.update_idletasks()
                try:
                    self.main_paned.sashpos(0, int(self.root.winfo_width() * 0.96))
                except Exception:
                    pass
                if hasattr(self, 'toggle_chat_btn'):
                    self.toggle_chat_btn.config(text="显示建议区")
            else:
                # 恢复为左侧占比更大，同时限制右侧最大宽度
                self.root.update_idletasks()
                try:
                    total_w = self.root.winfo_width()
                    max_right = int(total_w * 0.35)
                    self.main_paned.sashpos(0, total_w - max_right)
                except Exception:
                    pass
                if hasattr(self, 'toggle_chat_btn'):
                    self.toggle_chat_btn.config(text="隐藏建议区")
        except Exception:
            pass

    def on_main_resize(self, event):
        """窗口缩放时限制右侧最大宽度，避免遮挡主功能"""
        try:
            if getattr(self, 'chat_collapsed', False):
                return
            total_w = self.main_paned.winfo_width()
            left_w = self.main_paned.sashpos(0)
            right_w = max(total_w - left_w, 0)
            max_right = int(total_w * 0.35)
            if right_w > max_right:
                self.main_paned.sashpos(0, total_w - max_right)
        except Exception:
            pass

    def save_savings_data(self):
        """保存攒钱和目标数据到本地JSON文件"""
        try:
            data = dict(self.savings_data)

            def to_serializable(obj):
                if isinstance(obj, datetime):
                    return obj.isoformat()
                return obj

            # 序列化目标列表
            targets = []
            for t in data.get("targets", []):
                targets.append({
                    "name": t.get("name", ""),
                    "target_amount": float(t.get("target_amount", 0) or 0),
                    "current_amount": float(t.get("current_amount", 0) or 0),
                    "priority": t.get("priority", "中"),
                    "created_time": to_serializable(t.get("created_time", datetime.now())),
                    "history": t.get("history", [])
                })
            data["targets"] = targets

            # 序列化历史记录中的时间戳
            for key in ["savings_history", "amount_history", "price_history"]:
                items = []
                for h in data.get(key, []):
                    h_copy = dict(h)
                    if "timestamp" in h_copy and isinstance(h_copy["timestamp"], datetime):
                        h_copy["timestamp"] = h_copy["timestamp"].isoformat()
                    items.append(h_copy)
                data[key] = items

            with open(self.data_file, "w", encoding="utf-8") as f:
                json.dump(data, f, ensure_ascii=False, indent=2)
        except Exception:
            pass

    def load_savings_data(self):
        """从本地JSON文件加载攒钱和目标数据"""
        if not hasattr(self, "data_file"):
            self.data_file = os.path.join(os.path.dirname(__file__), "pxcs_data.json")
        if not os.path.exists(self.data_file):
            return
        try:
            with open(self.data_file, "r", encoding="utf-8") as f:
                data = json.load(f)

            # 合并默认结构
            for k in self.savings_data.keys():
                if k in data:
                    self.savings_data[k] = data[k]

            # 反序列化目标列表中的时间
            for t in self.savings_data.get("targets", []):
                ct = t.get("created_time")
                if isinstance(ct, str):
                    try:
                        t["created_time"] = datetime.fromisoformat(ct)
                    except Exception:
                        t["created_time"] = datetime.now()

            # 反序列化历史记录中的时间戳
            for key in ["savings_history", "amount_history", "price_history"]:
                for h in self.savings_data.get(key, []):
                    ts = h.get("timestamp")
                    if isinstance(ts, str):
                        try:
                            h["timestamp"] = datetime.fromisoformat(ts)
                        except Exception:
                            h["timestamp"] = datetime.now()
        except Exception:
            pass

    def create_targets_page(self):
        """创建目标管理页面"""
        # 添加新目标区域
        add_frame = ttk.LabelFrame(self.targets_frame, text="➕ 添加新目标", padding="12")
        add_frame.pack(fill=tk.X, pady=(0, 12))

        # 目标名称
        name_frame = ttk.Frame(add_frame)
        name_frame.pack(fill=tk.X, pady=5)
        ttk.Label(name_frame, text="目标名称:", width=10, font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 8))
        self.new_target_name = ttk.Entry(name_frame, width=30, font=('微软雅黑', 10))
        self.new_target_name.pack(side=tk.LEFT, padx=(0, 10))

        # 目标金额
        amount_frame = ttk.Frame(add_frame)
        amount_frame.pack(fill=tk.X, pady=5)
        ttk.Label(amount_frame, text="目标金额:", width=10, font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 8))
        self.new_target_amount = ttk.Entry(amount_frame, width=20, font=('微软雅黑', 10))
        self.new_target_amount.pack(side=tk.LEFT, padx=(0, 10))

        # 当前金额
        current_frame = ttk.Frame(add_frame)
        current_frame.pack(fill=tk.X, pady=5)
        ttk.Label(current_frame, text="当前已攒:", width=10, font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 8))
        self.new_target_current = ttk.Entry(current_frame, width=20, font=('微软雅黑', 10))
        self.new_target_current.pack(side=tk.LEFT, padx=(0, 10))
        self.new_target_current.insert(0, "0")

        # 优先级
        priority_frame = ttk.Frame(add_frame)
        priority_frame.pack(fill=tk.X, pady=5)
        ttk.Label(priority_frame, text="优先级:", width=10, font=('微软雅黑', 10)).pack(side=tk.LEFT, padx=(0, 8))
        self.new_target_priority = ttk.Combobox(priority_frame,
                                                values=["高", "中", "低"],
                                                state="readonly",
                                                width=10,
                                                font=('微软雅黑', 10))
        self.new_target_priority.pack(side=tk.LEFT, padx=(0, 10))
        self.new_target_priority.set("中")

        # 添加按钮
        button_frame = ttk.Frame(add_frame)
        button_frame.pack(fill=tk.X, pady=10)
        ttk.Button(button_frame,
                   text="➕ 添加目标",
                   command=self.add_new_target,
                   style='Success.TButton').pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame,
                   text="🔄 从AI查询添加",
                   command=self.add_target_from_ai,
                   style='Secondary.TButton').pack(side=tk.LEFT, padx=5)

        # 目标列表区域
        list_frame = ttk.LabelFrame(self.targets_frame, text="📋 目标列表", padding="12")
        list_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 12))

        # 创建目标列表树状图
        columns = ("名称", "目标金额", "当前金额", "进度", "优先级", "创建时间")
        self.targets_tree = ttk.Treeview(list_frame, columns=columns, show="headings", height=12)

        # 设置列宽
        self.targets_tree.column("名称", width=150)
        self.targets_tree.column("目标金额", width=100)
        self.targets_tree.column("当前金额", width=100)
        self.targets_tree.column("进度", width=80)
        self.targets_tree.column("优先级", width=80)
        self.targets_tree.column("创建时间", width=120)

        # 设置列标题
        for col in columns:
            self.targets_tree.heading(col, text=col)

        # 滚动条
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.targets_tree.yview)
        self.targets_tree.configure(yscrollcommand=scrollbar.set)

        self.targets_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        # 目标操作按钮
        action_frame = ttk.Frame(list_frame)
        action_frame.pack(fill=tk.X, pady=(10, 0))

        ttk.Button(action_frame,
                   text="🎯 设为当前目标",
                   command=self.set_as_current_target,
                   style='Primary.TButton').pack(side=tk.LEFT, padx=5)
        ttk.Button(action_frame,
                   text="✏️ 编辑目标",
                   command=self.edit_target,
                   style='Secondary.TButton').pack(side=tk.LEFT, padx=5)
        ttk.Button(action_frame,
                   text="❌ 删除目标",
                   command=self.delete_target,
                   style='Warning.TButton').pack(side=tk.LEFT, padx=5)
        ttk.Button(action_frame,
                   text="📊 查看进度",
                   command=self.view_target_progress,
                   style='Info.TButton').pack(side=tk.LEFT, padx=5)

        # 绑定双击事件
        self.targets_tree.bind('<Double-1>', self.on_target_double_click)

    def show_target_manager(self):
        """显示目标管理页面"""
        self.notebook.select(1)  # 切换到目标管理页
        self.refresh_targets_list()

    def refresh_targets(self):
        """刷新目标列表"""
        self.refresh_targets_list()
        self.update_target_combobox()

    def refresh_targets_list(self):
        """刷新目标列表显示"""
        # 清空现有列表
        for item in self.targets_tree.get_children():
            self.targets_tree.delete(item)

        # 添加目标到列表
        for i, target in enumerate(self.savings_data["targets"]):
            progress = (target["current_amount"] / target["target_amount"] * 100) if target["target_amount"] > 0 else 0
            progress_text = f"{progress:.1f}%"

            self.targets_tree.insert("", "end", values=(
                target["name"],
                f"¥{target['target_amount']:,.2f}",
                f"¥{target['current_amount']:,.2f}",
                progress_text,
                target["priority"],
                target["created_time"].strftime("%Y-%m-%d %H:%M")
            ))

    def update_target_combobox(self):
        """更新目标选择下拉框"""
        target_names = [target["name"] for target in self.savings_data["targets"]]
        self.target_combobox['values'] = target_names

        if self.current_target_index >= 0 and self.savings_data["targets"]:
            current_target = self.savings_data["targets"][self.current_target_index]
            self.target_combobox.set(current_target["name"])
            self.update_current_target_display()

    def add_new_target(self):
        """添加新目标"""
        name = self.new_target_name.get().strip()
        target_amount_str = self.new_target_amount.get().strip()
        current_amount_str = self.new_target_current.get().strip()
        priority = self.new_target_priority.get()

        if not name:
            messagebox.showwarning("输入错误", "请输入目标名称")
            return

        try:
            target_amount = float(target_amount_str) if target_amount_str else 0
            current_amount = float(current_amount_str) if current_amount_str else 0

            if target_amount <= 0:
                messagebox.showwarning("输入错误", "目标金额必须大于0")
                return

            new_target = {
                "name": name,
                "target_amount": target_amount,
                "current_amount": current_amount,
                "priority": priority,
                "created_time": datetime.now(),
                "history": []
            }

            self.savings_data["targets"].append(new_target)

            # 清空输入框
            self.new_target_name.delete(0, tk.END)
            self.new_target_amount.delete(0, tk.END)
            self.new_target_current.delete(0, tk.END)
            self.new_target_current.insert(0, "0")
            self.new_target_priority.set("中")

            # 刷新显示
            self.refresh_targets_list()
            self.update_target_combobox()
            self.save_savings_data()

            messagebox.showinfo("成功", f"已添加目标：{name}")
            self.add_message("系统", f"🎯 已添加新目标：{name}（目标金额：¥{target_amount:,.2f}）", "system")

        except ValueError:
            messagebox.showerror("输入错误", "请输入有效的金额数字")

    def add_target_from_ai(self):
        """通过AI查询添加目标"""
        name = self.new_target_name.get().strip()
        if not name:
            messagebox.showwarning("输入错误", "请输入目标物品名称")
            return

        self.ai_query_price_for_new_target(name)

    def ai_query_price_for_new_target(self, item_name):
        """为新增目标查询价格"""
        self.status_var.set("🟡 AI正在查询物品价格...")
        self.ai_progress_bar.pack(fill=tk.X, pady=5)
        self.ai_progress_bar.start()

        future = self.thread_pool.submit(self.get_item_price, item_name)
        request_id = id(future)
        self.active_requests.add(request_id)
        future.add_done_callback(lambda f: self.handle_price_query_for_new_target(f, request_id, item_name))

    def handle_price_query_for_new_target(self, future, request_id, item_name):
        """处理新目标的价格查询结果"""
        self.active_requests.discard(request_id)
        self.ai_progress_bar.stop()
        self.ai_progress_bar.pack_forget()

        try:
            result, error = future.result()

            if error:
                messagebox.showerror("查询失败", f"AI查询价格时发生错误：{error}")
            else:
                price = self.extract_price_from_response(result)
                if price:
                    self.new_target_name.delete(0, tk.END)
                    self.new_target_name.insert(0, item_name)
                    self.new_target_amount.delete(0, tk.END)
                    self.new_target_amount.insert(0, str(price))
                    self.new_target_current.delete(0, tk.END)
                    self.new_target_current.insert(0, "0")

                    messagebox.showinfo("查询成功", f"已为'{item_name}'查询到建议价格：¥{price:,.2f}")
                    self.add_message("AI助手", f"🎯 已为'{item_name}'查询到建议价格：¥{price:,.2f}", "assistant")
                else:
                    messagebox.showwarning("查询结果", "无法从AI回复中提取明确价格，请手动输入")

        except Exception as e:
            messagebox.showerror("处理错误", f"处理查询结果时发生错误：{str(e)}")
        finally:
            self.status_var.set("🟢 系统就绪")

    def set_as_current_target(self):
        """设为当前目标"""
        selection = self.targets_tree.selection()
        if not selection:
            messagebox.showwarning("选择错误", "请先选择一个目标")
            return

        item = selection[0]
        index = self.targets_tree.index(item)

        if 0 <= index < len(self.savings_data["targets"]):
            self.current_target_index = index
            target = self.savings_data["targets"][index]

            # 更新主页面显示
            self.item_entry.delete(0, tk.END)
            self.item_entry.insert(0, target["name"])

            self.current_entry.delete(0, tk.END)
            self.current_entry.insert(0, str(target["current_amount"]))

            self.savings_data["target_item"] = target["name"]
            self.savings_data["target_amount"] = target["target_amount"]
            self.savings_data["current_amount"] = target["current_amount"]

            self.update_target_combobox()
            self.update_progress_display()
            self.save_savings_data()

            messagebox.showinfo("成功", f"已设置当前目标为：{target['name']}")
            self.add_message("系统", f"🎯 已切换当前目标为：{target['name']}", "system")

    def on_target_selected(self, event):
        """当从下拉框选择目标时"""
        selected_name = self.target_combobox.get()
        for i, target in enumerate(self.savings_data["targets"]):
            if target["name"] == selected_name:
                self.current_target_index = i
                self.set_as_current_target()
                break

    def on_target_double_click(self, event):
        """双击目标时设为当前目标"""
        self.set_as_current_target()

    def edit_target(self):
        """编辑目标"""
        selection = self.targets_tree.selection()
        if not selection:
            messagebox.showwarning("选择错误", "请先选择一个目标")
            return

        item = selection[0]
        index = self.targets_tree.index(item)
        target = self.savings_data["targets"][index]

        # 创建编辑对话框
        self.create_edit_dialog(target, index)

    def create_edit_dialog(self, target, index):
        """创建编辑目标对话框"""
        edit_window = tk.Toplevel(self.root)
        edit_window.title("编辑目标")
        edit_window.geometry("400x300")
        edit_window.configure(bg='#2C3E50')
        edit_window.transient(self.root)
        edit_window.grab_set()

        main_frame = ttk.Frame(edit_window, padding="20")
        main_frame.pack(fill=tk.BOTH, expand=True)

        ttk.Label(main_frame, text="目标名称:", font=('微软雅黑', 10)).pack(anchor=tk.W, pady=5)
        name_entry = ttk.Entry(main_frame, width=30, font=('微软雅黑', 10))
        name_entry.pack(fill=tk.X, pady=5)
        name_entry.insert(0, target["name"])

        ttk.Label(main_frame, text="目标金额:", font=('微软雅黑', 10)).pack(anchor=tk.W, pady=5)
        amount_entry = ttk.Entry(main_frame, width=30, font=('微软雅黑', 10))
        amount_entry.pack(fill=tk.X, pady=5)
        amount_entry.insert(0, str(target["target_amount"]))

        ttk.Label(main_frame, text="当前金额:", font=('微软雅黑', 10)).pack(anchor=tk.W, pady=5)
        current_entry = ttk.Entry(main_frame, width=30, font=('微软雅黑', 10))
        current_entry.pack(fill=tk.X, pady=5)
        current_entry.insert(0, str(target["current_amount"]))

        ttk.Label(main_frame, text="优先级:", font=('微软雅黑', 10)).pack(anchor=tk.W, pady=5)
        priority_combo = ttk.Combobox(main_frame,
                                      values=["高", "中", "低"],
                                      state="readonly",
                                      width=10,
                                      font=('微软雅黑', 10))
        priority_combo.pack(fill=tk.X, pady=5)
        priority_combo.set(target["priority"])

        def save_changes():
            try:
                target["name"] = name_entry.get().strip()
                target["target_amount"] = float(amount_entry.get())
                target["current_amount"] = float(current_entry.get())
                target["priority"] = priority_combo.get()

                self.refresh_targets_list()
                self.update_target_combobox()
                self.update_progress_display()
                self.save_savings_data()

                edit_window.destroy()
                messagebox.showinfo("成功", "目标已更新")
                self.add_message("系统", f"✏️ 已更新目标：{target['name']}", "system")

            except ValueError:
                messagebox.showerror("输入错误", "请输入有效的金额数字")

        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill=tk.X, pady=20)

        ttk.Button(button_frame,
                   text="💾 保存",
                   command=save_changes,
                   style='Success.TButton').pack(side=tk.LEFT, padx=10)
        ttk.Button(button_frame,
                   text="❌ 取消",
                   command=edit_window.destroy,
                   style='Warning.TButton').pack(side=tk.LEFT, padx=10)

    def delete_target(self):
        """删除目标"""
        selection = self.targets_tree.selection()
        if not selection:
            messagebox.showwarning("选择错误", "请先选择一个目标")
            return

        item = selection[0]
        index = self.targets_tree.index(item)
        target = self.savings_data["targets"][index]

        if messagebox.askyesno("确认删除", f"确定要删除目标 '{target['name']}' 吗？"):
            # 如果删除的是当前目标，清空当前目标
            if index == self.current_target_index:
                self.current_target_index = -1
                self.clear_current_target()

            self.savings_data["targets"].pop(index)
            self.refresh_targets_list()
            self.update_target_combobox()
            self.save_savings_data()

            messagebox.showinfo("成功", "目标已删除")
            self.add_message("系统", f"❌ 已删除目标：{target['name']}", "system")

    def view_target_progress(self):
        """查看目标进度"""
        selection = self.targets_tree.selection()
        if not selection:
            messagebox.showwarning("选择错误", "请先选择一个目标")
            return

        item = selection[0]
        index = self.targets_tree.index(item)
        target = self.savings_data["targets"][index]

        progress = (target["current_amount"] / target["target_amount"] * 100) if target["target_amount"] > 0 else 0
        remaining = max(target["target_amount"] - target["current_amount"], 0)

        progress_info = f"""
🎯 目标：{target['name']}
💰 目标金额：¥{target['target_amount']:,.2f}
💵 当前已攒：¥{target['current_amount']:,.2f}
📊 完成进度：{progress:.1f}%
🎯 还需攒：¥{remaining:,.2f}
⭐ 优先级：{target['priority']}
📅 创建时间：{target['created_time'].strftime('%Y-%m-%d %H:%M')}
        """

        messagebox.showinfo("目标进度", progress_info)

    def clear_current_target(self):
        """清空当前目标"""
        self.item_entry.delete(0, tk.END)
        self.current_entry.delete(0, tk.END)
        self.savings_data["target_item"] = ""
        self.savings_data["target_amount"] = 0.0
        self.savings_data["current_amount"] = 0.0
        self.update_progress_display()
        self.current_target_label.config(text="暂无选中目标")
        self.save_savings_data()

    def update_current_target_display(self):
        """更新当前目标显示"""
        if self.current_target_index >= 0 and self.savings_data["targets"]:
            target = self.savings_data["targets"][self.current_target_index]
            progress = (target["current_amount"] / target["target_amount"] * 100) if target["target_amount"] > 0 else 0
            self.current_target_label.config(
                text=f"当前目标：{target['name']} | 进度：{progress:.1f}% | 优先级：{target['priority']}",
                foreground='#2ECC71'
            )
        else:
            self.current_target_label.config(text="暂无选中目标", foreground='#F39C12')

    def set_target(self):
        """确认设置攒钱目标，并自动添加到目标管理"""
        try:
            item = self.item_entry.get().strip()
            current = float(self.current_entry.get() or 0)
            income = float(self.income_entry.get() or 0)
            expenses = float(self.expenses_entry.get() or 0)

            if not item:
                messagebox.showwarning("输入错误", "请输入目标物品名称")
                return

            if self.savings_data["target_amount"] <= 0:
                messagebox.showwarning("设置错误", "请先通过AI查询价格或确保已设置目标金额")
                return

            # 检查是否已存在同名目标
            existing_target_index = -1
            for i, target in enumerate(self.savings_data["targets"]):
                if target["name"] == item:
                    existing_target_index = i
                    break

            if existing_target_index >= 0:
                # 目标已存在，询问是否更新
                if messagebox.askyesno("目标已存在",
                                       f"目标 '{item}' 已存在，是否更新该目标？\n\n"
                                       f"原目标金额：¥{self.savings_data['targets'][existing_target_index]['target_amount']:,.2f}\n"
                                       f"新目标金额：¥{self.savings_data['target_amount']:,.2f}"):
                    # 更新现有目标
                    self.savings_data["targets"][existing_target_index].update({
                        "target_amount": self.savings_data["target_amount"],
                        "current_amount": current,
                        "priority": "中"  # 重置为默认优先级
                    })
                    # 设为当前目标
                    self.current_target_index = existing_target_index

                    messagebox.showinfo("更新成功", f"已更新目标：{item}")
                    self.add_message("系统", f"✏️ 已更新现有目标：{item}", "system")
                else:
                    return
            else:
                # 创建新目标并添加到目标管理
                new_target = {
                    "name": item,
                    "target_amount": self.savings_data["target_amount"],
                    "current_amount": current,
                    "priority": "中",
                    "created_time": datetime.now(),
                    "history": []
                }

                self.savings_data["targets"].append(new_target)
                self.current_target_index = len(self.savings_data["targets"]) - 1

                messagebox.showinfo("添加成功", f"已添加新目标到目标管理：{item}")
                self.add_message("系统", f"🎯 已添加新目标到目标管理：{item}", "system")

            # 更新主要数据
            self.savings_data.update({
                "target_item": item,
                "current_amount": current,
                "monthly_income": income,
                "monthly_expenses": expenses
            })

            # 初始化金额记录
            self.savings_data["amount_history"].append({
                "timestamp": datetime.now(),
                "amount": current,
                "target_amount": self.savings_data["target_amount"]
            })

            # 添加初始操作历史
            self.savings_data["savings_history"].append({
                "timestamp": datetime.now(),
                "amount": current,
                "type": "初始设置",
                "balance_after": current
            })

            # 刷新目标管理显示
            self.refresh_targets_list()
            self.update_target_combobox()

            self.add_message("系统",
                             f"🎯 已确认攒钱目标：{item}\n"
                             f"💰 目标金额：¥{self.savings_data['target_amount']:,.2f}\n"
                             f"💵 当前已攒：¥{current:,.2f}\n"
                             f"📊 月净收入：¥{income - expenses:,.2f}", "system")
            self.update_progress_display()
            self.get_savings_advice()
            self.save_savings_data()

        except ValueError:
            messagebox.showerror("输入错误", "请输入有效的数字")

    def start_amount_recording(self):
        """启动金额记录线程"""
        self.amount_recording_running = True
        self.amount_thread = threading.Thread(target=self.record_amount_changes, daemon=True)
        self.amount_thread.start()

        # 立即记录一次初始数据
        if self.savings_data["current_amount"] > 0:
            self.savings_data["amount_history"].append({
                "timestamp": datetime.now(),
                "amount": self.savings_data["current_amount"],
                "target_amount": self.savings_data["target_amount"]
            })

    def record_amount_changes(self):
        """记录每刻金额变化"""
        while self.amount_recording_running:
            time.sleep(60)  # 每分钟记录一次
            if self.savings_data["current_amount"] > 0:
                self.savings_data["amount_history"].append({
                    "timestamp": datetime.now(),
                    "amount": self.savings_data["current_amount"],
                    "target_amount": self.savings_data["target_amount"]
                })

    def export_to_excel(self):
        """导出数据到Excel"""
        try:
            # 检查是否有数据可导出
            if (not self.savings_data["savings_history"] and
                    not self.savings_data["amount_history"] and
                    not self.savings_data["targets"]):
                messagebox.showwarning("导出失败", "没有数据可导出")
                return

            # 生成安全的文件名
            try:
                # 获取桌面路径
                if os.name == 'nt':  # Windows
                    desktop = os.path.join(os.path.expanduser("~"), "Desktop")
                else:  # Mac/Linux
                    desktop = os.path.join(os.path.expanduser("~"), "Desktop")

                # 如果桌面目录不存在，使用当前目录
                if not os.path.exists(desktop):
                    desktop = "."

                filename = f"貔貅计划数据_{datetime.now().strftime('%Y%m%d_%H%M%S')}.xlsx"
                filepath = os.path.join(desktop, filename)

            except Exception:
                # 如果获取桌面路径失败，使用当前目录
                filename = f"貔貅计划数据_{datetime.now().strftime('%Y%m%d_%H%M%S')}.xlsx"
                filepath = filename

            # 导出到Excel，添加错误处理
            try:
                with pd.ExcelWriter(filepath, engine='openpyxl') as writer:
                    # 导出当前目标数据
                    if self.savings_data["savings_history"] or self.savings_data["amount_history"]:
                        data = []

                        # 添加基本信息
                        if self.savings_data["target_item"]:
                            data.append({
                                "时间": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                                "类型": "基本信息",
                                "目标物品": self.savings_data["target_item"],
                                "目标金额": self.savings_data["target_amount"],
                                "当前金额": self.savings_data["current_amount"],
                                "月收入": self.savings_data["monthly_income"],
                                "月支出": self.savings_data["monthly_expenses"],
                                "数据来源": "系统信息"
                            })

                        # 添加操作历史
                        for history in self.savings_data["savings_history"]:
                            data.append({
                                "时间": history["timestamp"].strftime("%Y-%m-%d %H:%M:%S"),
                                "类型": history["type"],
                                "金额": history["amount"],
                                "操作后余额": history["balance_after"],
                                "数据来源": "操作记录"
                            })

                        # 添加金额变化历史
                        for amount_record in self.savings_data["amount_history"]:
                            data.append({
                                "时间": amount_record["timestamp"].strftime("%Y-%m-%d %H:%M:%S"),
                                "类型": "金额记录",
                                "当前金额": amount_record["amount"],
                                "目标金额": amount_record["target_amount"],
                                "完成进度": f"{(amount_record['amount'] / amount_record['target_amount'] * 100) if amount_record['target_amount'] > 0 else 0:.1f}%",
                                "数据来源": "自动记录"
                            })

                        if data:
                            df = pd.DataFrame(data)
                            df.to_excel(writer, sheet_name='当前目标数据', index=False)

                    # 导出多目标数据
                    if self.savings_data["targets"]:
                        targets_data = []
                        for target in self.savings_data["targets"]:
                            progress = (target["current_amount"] / target["target_amount"] * 100) if target[
                                                                                                         "target_amount"] > 0 else 0
                            targets_data.append({
                                "目标名称": target["name"],
                                "目标金额": target["target_amount"],
                                "当前金额": target["current_amount"],
                                "完成进度": f"{progress:.1f}%",
                                "还需金额": max(target["target_amount"] - target["current_amount"], 0),
                                "优先级": target["priority"],
                                "创建时间": target["created_time"].strftime("%Y-%m-%d %H:%M")
                            })

                        df_targets = pd.DataFrame(targets_data)
                        df_targets.to_excel(writer, sheet_name='多目标管理', index=False)

                messagebox.showinfo("导出成功", f"数据已成功导出到：\n{filepath}")
                self.add_message("系统", f"📊 数据已成功导出到：{filepath}", "system")

            except PermissionError:
                messagebox.showerror("导出失败", f"文件被占用或无写入权限：\n{filepath}\n请关闭文件或选择其他位置。")
            except Exception as e:
                messagebox.showerror("导出失败", f"导出文件时发生错误：\n{str(e)}")

        except ImportError:
            messagebox.showerror("导出失败", "缺少必要的库：openpyxl\n请安装：pip install openpyxl")
        except Exception as e:
            messagebox.showerror("导出失败", f"导出数据时发生未知错误：{str(e)}")

    def show_charts(self):
        """显示图表窗口"""
        if not self.savings_data["amount_history"]:
            messagebox.showwarning("图表显示", "没有足够的数据显示图表")
            return

        # 创建图表窗口
        chart_window = tk.Toplevel(self.root)
        chart_window.title("📈 攒钱进度图表")
        chart_window.geometry("800x600")
        chart_window.configure(bg='#2C3E50')

        # 设置中文字体
        plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'DejaVu Sans']
        plt.rcParams['axes.unicode_minus'] = False

        # 创建图表
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))
        fig.patch.set_facecolor('#2C3E50')

        # 准备数据
        timestamps = [record["timestamp"] for record in self.savings_data["amount_history"]]
        amounts = [record["amount"] for record in self.savings_data["amount_history"]]
        target_amounts = [record["target_amount"] for record in self.savings_data["amount_history"]]

        # 绘制金额变化图
        ax1.plot(timestamps, amounts, 'o-', color='#1ABC9C', linewidth=2, markersize=4, label='当前金额')
        ax1.axhline(y=self.savings_data["target_amount"], color='#E74C3C', linestyle='--', label='目标金额')
        ax1.set_facecolor('#34495E')
        ax1.set_title('💰 金额变化趋势', color='white', fontsize=14, pad=20)
        ax1.set_ylabel('金额 (元)', color='white')
        ax1.tick_params(axis='x', rotation=45, colors='white')
        ax1.tick_params(axis='y', colors='white')
        ax1.legend(facecolor='#34495E', edgecolor='none', labelcolor='white', loc='best')
        ax1.grid(True, alpha=0.3)

        # 绘制进度百分比图
        progress_data = [(amount / target) * 100 for amount, target in zip(amounts, target_amounts) if target > 0]
        if progress_data:
            ax2.plot(timestamps[:len(progress_data)], progress_data, 's-', color='#3498DB', linewidth=2, markersize=4,
                     label='完成进度')
            ax2.set_facecolor('#34495E')
            ax2.set_title('📊 完成进度百分比', color='white', fontsize=14, pad=20)
            ax2.set_ylabel('完成度 (%)', color='white')
            ax2.set_xlabel('时间', color='white')
            ax2.tick_params(axis='x', rotation=45, colors='white')
            ax2.tick_params(axis='y', colors='white')
            ax2.legend(facecolor='#34495E', edgecolor='none', labelcolor='white', loc='best')
            ax2.grid(True, alpha=0.3)
            ax2.set_ylim(0, 100)

        plt.tight_layout()

        # 嵌入到Tkinter窗口
        canvas = FigureCanvasTkAgg(fig, chart_window)
        canvas.draw()
        canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # 添加关闭按钮
        close_button = ttk.Button(chart_window, text="关闭", command=chart_window.destroy, style='Warning.TButton')
        close_button.pack(pady=10)

    def update_monthly_savings_display(self, event=None):
        """更新月储蓄显示"""
        try:
            income = float(self.income_entry.get() or 0)
            expenses = float(self.expenses_entry.get() or 0)
            monthly_savings = income - expenses

            if monthly_savings > 0:
                self.monthly_savings_label.config(
                    text=f"月净收入：+¥{monthly_savings:,.2f}",
                    foreground='#2ECC71'
                )
            elif monthly_savings < 0:
                self.monthly_savings_label.config(
                    text=f"月净收入：-¥{abs(monthly_savings):,.2f}",
                    foreground='#E74C3C'
                )
            else:
                self.monthly_savings_label.config(
                    text="月净收入：¥0.00",
                    foreground='#BDC3C7'
                )
        except ValueError:
            self.monthly_savings_label.config(text="")

    def add_income_to_savings(self):
        """将月收入扣除月支出后的净收入添加到已攒金额"""
        try:
            income = float(self.income_entry.get() or 0)
            expenses = float(self.expenses_entry.get() or 0)

            if income <= 0:
                messagebox.showwarning("输入错误", "月收入必须大于0")
                return

            if expenses < 0:
                messagebox.showwarning("输入错误", "月支出不能为负数")
                return

            # 计算净收入
            net_income = income - expenses

            if net_income <= 0:
                messagebox.showwarning("操作错误", f"净收入为¥{net_income:,.2f}，无法添加到已攒金额")
                return

            # 检查是否已经添加过本月收入
            current_month = datetime.now().strftime("%Y-%m")
            if (self.savings_data["last_income_update"] == current_month and
                    self.savings_data["monthly_income"] == income and
                    self.savings_data["monthly_expenses"] == expenses):
                if not messagebox.askyesno("确认操作", "本月工资已到账过，确定要再次添加吗？"):
                    return

            old_amount = self.savings_data["current_amount"]
            self.savings_data["current_amount"] += net_income
            self.savings_data["monthly_income"] = income
            self.savings_data["monthly_expenses"] = expenses
            self.savings_data["last_income_update"] = current_month

            # 更新当前已攒输入框
            self.current_entry.delete(0, tk.END)
            self.current_entry.insert(0, f"{self.savings_data['current_amount']:.2f}")

            # 如果当前有选中目标，也更新目标数据
            if self.current_target_index >= 0:
                target = self.savings_data["targets"][self.current_target_index]
                target["current_amount"] = self.savings_data["current_amount"]
                self.refresh_targets_list()

            # 记录操作历史
            self.savings_data["savings_history"].append({
                "timestamp": datetime.now(),
                "amount": net_income,
                "type": "工资净收入",
                "balance_after": self.savings_data["current_amount"],
                "details": f"收入: ¥{income:,.2f} - 支出: ¥{expenses:,.2f} = 净收入: ¥{net_income:,.2f}"
            })

            # 记录金额变化
            self.savings_data["amount_history"].append({
                "timestamp": datetime.now(),
                "amount": self.savings_data["current_amount"],
                "target_amount": self.savings_data["target_amount"]
            })
            self.save_savings_data()

            # 显示详细的操作信息
            operation_details = (
                f"💵 工资到账明细：\n"
                f"• 月收入：+¥{income:,.2f}\n"
                f"• 月支出：-¥{expenses:,.2f}\n"
                f"• 净收入：+¥{net_income:,.2f}\n"
                f"💰 余额更新：¥{old_amount:,.2f} → ¥{self.savings_data['current_amount']:,.2f}"
            )

            self.add_message("系统", operation_details, "system")
            self.update_progress_display()

        except ValueError:
            messagebox.showerror("输入错误", "请输入有效的数字")

    def is_car_related(self, item_name):
        """判断物品是否与汽车相关"""
        car_keywords = [
            '汽车', '车', '轿车', 'SUV', '越野', '电动车', '新能源', '特斯拉', '宝马', '奔驰',
            '奥迪', '丰田', '本田', '大众', '福特', '别克', '现代', '起亚', '沃尔沃', '雷克萨斯',
            '比亚迪', '吉利', '长安', '长城', '奇瑞', '蔚来', '小鹏', '理想', '保时捷', '路虎',
            '捷豹', '玛莎拉蒂', '兰博基尼', '法拉利', '宾利', '劳斯莱斯', '迈巴赫', '红旗'
        ]
        return any(keyword in item_name for keyword in car_keywords)

    def query_price(self):
        """根据选择的查询方式查询价格"""
        item_name = self.item_entry.get().strip()
        search_method = self.search_method.get()

        if not item_name:
            messagebox.showwarning("输入错误", "请输入目标物品名称")
            return

        self.status_var.set(f"🟡 正在通过{search_method}查询价格...")
        self.ai_progress_bar.pack(fill=tk.X, pady=5)
        self.ai_progress_bar.start()

        # 根据查询方式选择不同的查询方法
        if search_method == "汽车之家":
            # 检查是否为汽车相关物品
            if self.is_car_related(item_name):
                future = self.thread_pool.submit(self.crawl_autohome_price, item_name)
            else:
                messagebox.showinfo("提示", "非汽车类物品，将使用AI查询")
                future = self.thread_pool.submit(self.get_item_price, item_name)
        elif search_method == "慢慢买":
            future = self.thread_pool.submit(self.crawl_manmanbuy_price, item_name)
        else:  # AI查询
            future = self.thread_pool.submit(self.get_item_price, item_name)

        request_id = id(future)
        self.active_requests.add(request_id)
        future.add_done_callback(lambda f: self.handle_price_query_result(f, request_id, search_method))

    def crawl_autohome_price(self, item_name):
        """爬取汽车之家价格 - 使用实际API"""
        try:
            headers = {
                'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36 Edg/141.0.0.0'
            }

            # 构建API请求URL
            encoded_car_name = urllib.parse.quote(item_name)
            url = f'https://sou.api.autohome.com.cn/v1/search?uuid=c1aad40a-27a1-473d-94b8-9935854bdcd0&source=pc&is_base_exp=0&modify=0&q={encoded_car_name}&entry=42&error=0&pvareaid=6861421&mq=&charset=utf8&pid=90300023&offset=0&size=10&page=1&ext=%7B%22chl%22:%22%22,%22plat%22:%22pc%22,%22pf%22:%22h5%22,%22bbsId%22:%22%22,%22q%22:%22{encoded_car_name}%22,%22offset%22:0,%22size%22:10,%22modify%22:%220%22,%22cityid%22:230100,%22perscont%22:%221%22,%22version%22:%221.0.3%22,%22box_count%22:0%7D'

            response = requests.get(url, headers=headers, timeout=10)
            response.encoding = 'utf-8'
            result = response.json()

            # 检查API响应是否有效
            if (result.get('result') and
                    result['result'].get('itemlist') and
                    len(result['result']['itemlist']) > 0):

                item_info = result['result']['itemlist'][0]['iteminfo']['data']
                max_price = item_info.get('local_series_max_price', 0)
                min_price = item_info.get('local_series_min_price', 0)

                # 计算平均价格
                if min_price > 0 and max_price > 0:
                    avg_price = (min_price + max_price) / 2
                elif min_price > 0:
                    avg_price = min_price
                elif max_price > 0:
                    avg_price = max_price
                else:
                    return None, None, "未找到有效的价格信息"

                price_info = f"""
🚗 汽车之家价格查询结果：

📋 物品名称：{item_name}
💰 最低价格：¥{min_price:,.2f}
💰 最高价格：¥{max_price:,.2f}
💵 建议价格：¥{avg_price:,.2f}
📊 数据来源：汽车之家官方API

💡 提示：实际价格可能因配置、地区、促销等因素有所不同，建议到店咨询。
                """

                return price_info, avg_price, None
            else:
                return None, None, "未找到相关车型信息"

        except requests.exceptions.Timeout:
            return None, None, "请求超时，请检查网络连接"
        except requests.exceptions.RequestException as e:
            return None, None, f"网络请求失败：{str(e)}"
        except (KeyError, IndexError) as e:
            return None, None, f"解析数据失败：{str(e)}"
        except Exception as e:
            return None, None, f"汽车之家爬虫失败：{str(e)}"

    def crawl_manmanbuy_price(self, item_name):
        """爬取慢慢买价格（移动搜索页）"""
        try:
            headers = {
                'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36 Edg/141.0.0.0'
            }
            search_url = f"https://s.manmanbuy.com/m/search/result?keyword={urllib.parse.quote(item_name)}"

            response = requests.get(search_url, headers=headers, timeout=10)
            response.encoding = 'utf-8'
            html_text = response.text

            # 在页面中尝试匹配多个价格模式，尽量稳健
            price_patterns = [
                r'"thirdPartyCurrentPrice"\s*:\s*([0-9]+(?:\.[0-9]+)?)',
                r'"children":"\s*([0-9]{2,6})(?:\.[0-9]+)?\s*元"',
                r'>\s*([0-9]{2,6})(?:\.[0-9]+)?\s*元\s*<',
                r'¥\s*([0-9]{2,6}(?:\.[0-9]+)?)'
            ]

            prices = []
            for pattern in price_patterns:
                try:
                    matches = re.findall(pattern, html_text)
                    for m in matches:
                        val = float(str(m))
                        prices.append(val)
                except Exception:
                    continue

            prices = [p for p in prices if p > 10]

            if not prices:
                return None, None, "未在慢慢买页面解析到价格"

            price = prices[0]

            price_info = f"""
🛒 慢慢买价格查询结果：

📋 物品名称：{item_name}
💵 建议价格：¥{price:,.2f}
📊 数据来源：慢慢买（搜索页解析）

💡 提示：页面结构可能变化，建议结合商家页面进一步核对。
            """

            return price_info, price, None

        except requests.exceptions.Timeout:
            return None, None, "请求超时，请检查网络连接"
        except requests.exceptions.RequestException as e:
            return None, None, f"网络请求失败：{str(e)}"
        except Exception as e:
            return None, None, f"慢慢买爬虫失败：{str(e)}"

    def handle_price_query_result(self, future, request_id, search_method):
        """处理价格查询结果"""
        self.active_requests.discard(request_id)
        self.ai_progress_bar.stop()
        self.ai_progress_bar.pack_forget()

        try:
            if search_method in ["汽车之家", "慢慢买"]:
                # 处理爬虫结果
                result, price, error = future.result()

                if error:
                    self.root.after(0, lambda: self.price_info_label.config(
                        text=f"❌ {search_method}查询失败：{error}",
                        foreground='#E74C3C'
                    ))
                    # 提供使用AI查询的选项
                    if messagebox.askyesno("查询失败",
                                           f"{search_method}查询失败：{error}\n\n是否使用AI查询替代？"):
                        self.search_method.set("AI查询")
                        self.ai_query_price()
                else:
                    self.savings_data["item_price"] = price
                    self.savings_data["target_amount"] = price
                    self.root.after(0, lambda: self.price_info_label.config(
                        text=f"✅ {search_method}建议价格：¥{price:,.2f}（已自动设置为目标金额）",
                        foreground='#2ECC71'
                    ))
                    self.add_message(f"{search_method}爬虫",
                                     f"🎯 已为'{self.item_entry.get()}'查询到建议价格：¥{price:,.2f}\n\n{result}",
                                     "assistant")
            else:
                # 处理AI查询结果
                result, error = future.result()

                if error:
                    self.root.after(0, lambda: self.price_info_label.config(
                        text=f"❌ AI查询失败：{error}",
                        foreground='#E74C3C'
                    ))
                else:
                    # 解析价格信息并提取数字
                    price = self.extract_price_from_response(result)
                    if price:
                        self.savings_data["item_price"] = price
                        self.savings_data["target_amount"] = price
                        self.root.after(0, lambda: self.price_info_label.config(
                            text=f"✅ AI建议价格：¥{price:,.2f}（已自动设置为目标金额）",
                            foreground='#2ECC71'
                        ))
                        self.add_message("AI助手",
                                         f"🎯 已为'{self.item_entry.get()}'查询到建议价格：¥{price:,.2f}\n\n📋 详细价格信息：{result}",
                                         "assistant")
                    else:
                        self.root.after(0, lambda: self.price_info_label.config(
                            text="⚠️ 无法从AI回复中提取明确价格，请手动输入",
                            foreground='#F39C12'
                        ))
                        self.add_message("AI助手",
                                         f"📊 价格查询结果：{result}\n\n❓ 未能提取明确价格，请参考上述信息手动设置目标金额。",
                                         "assistant")

        except Exception as e:
            self.root.after(0, lambda: self.price_info_label.config(
                text=f"❌ 处理结果时发生错误：{str(e)}",
                foreground='#E74C3C'
            ))

        finally:
            self.root.after(0, lambda: self.status_var.set("🟢 系统就绪"))

    def ai_query_price(self):
        """通过AI查询物品价格（内部调用）"""
        item_name = self.item_entry.get().strip()

        if not item_name:
            return

        self.status_var.set("🟡 AI正在查询物品价格...")
        self.ai_progress_bar.pack(fill=tk.X, pady=5)
        self.ai_progress_bar.start()

        # 使用线程池提交任务
        future = self.thread_pool.submit(self.get_item_price, item_name)
        request_id = id(future)
        self.active_requests.add(request_id)

        # 添加回调处理结果
        future.add_done_callback(lambda f: self.handle_price_query_result(f, request_id, "AI查询"))

    def get_item_price(self, item_name):
        """获取物品价格 - 在单独的线程中运行"""
        try:
            # 模拟AI思考过程
            time.sleep(1)  # 模拟网络延迟

            prompt = f"""
            请查询以下物品的当前市场价格：{item_name}

            请提供：
            1. 物品的典型价格范围（人民币）
            2. 主要品牌或型号的价格差异
            3. 建议的合理购买价格
            4. 价格信息来源说明

            请用中文回复，价格信息要准确合理。
            """

            response = self.client.chat.completions.create(
                model="deepseek-chat",
                messages=[
                    {"role": "system", "content": "你是一个专业的市场调研专家，擅长查询各种商品的市场价格。"},
                    {"role": "user", "content": prompt},
                ],
                stream=False
            )

            return response.choices[0].message.content, None

        except Exception as e:
            return None, str(e)

    def extract_price_from_response(self, response):
        """从AI回复中提取价格数字"""
        # 查找价格模式：¥1000、1000元、1000-2000元等
        price_patterns = [
            r'¥\s*(\d+(?:,\d+)*(?:\.\d+)?)',  # ¥1,000.00
            r'(\d+(?:,\d+)*(?:\.\d+)?)\s*元',  # 1000元
            r'(\d+(?:,\d+)*(?:\.\d+)?)\s*人民币',  # 1000人民币
            r'价格.*?(\d+(?:,\d+)*(?:\.\d+)?)',  # 价格1000
        ]

        for pattern in price_patterns:
            matches = re.findall(pattern, response)
            if matches:
                # 取第一个匹配的价格
                price_str = matches[0].replace(',', '')
                try:
                    price = float(price_str)
                    # 如果价格看起来不合理（太小），可能是部分价格，尝试找更大的数字
                    if price < 10:
                        continue
                    return price
                except ValueError:
                    continue

        # 如果没找到明确价格，尝试查找价格范围
        range_pattern = r'(\d+)\s*[-~]\s*(\d+)'
        range_matches = re.findall(range_pattern, response)
        if range_matches:
            try:
                # 取价格范围的平均值
                min_price = float(range_matches[0][0])
                max_price = float(range_matches[0][1])
                return (min_price + max_price) / 2
            except ValueError:
                pass

        return None

    def start_price_monitoring(self):
        """启动价格监控线程"""
        self.price_monitor_running = True
        self.price_thread = threading.Thread(target=self.monitor_price_changes, daemon=True)
        self.price_thread.start()

    def monitor_price_changes(self):
        """监控价格变化（模拟）"""
        while self.price_monitor_running:
            time.sleep(30)  # 每30秒检查一次
            if self.savings_data["item_price"] > 0:
                # 模拟价格波动 (-3% 到 +5%)
                change_percent = random.uniform(-0.03, 0.05)
                new_price = self.savings_data["item_price"] * (1 + change_percent)
                self.savings_data["item_price"] = new_price
                self.savings_data["target_amount"] = new_price  # 同步更新目标金额
                self.savings_data["price_history"].append({
                    "timestamp": datetime.now(),
                    "price": new_price
                })

                # 更新界面
                self.root.after(0, self.update_progress_display)

    def add_amount(self):
        """增加金额"""
        self.modify_amount("add")

    def subtract_amount(self):
        """减少金额"""
        self.modify_amount("subtract")

    def modify_amount(self, operation):
        """修改金额"""
        try:
            amount = float(self.amount_entry.get() or 0)
            if amount <= 0:
                messagebox.showwarning("输入错误", "操作金额必须大于0")
                return

            old_amount = self.savings_data["current_amount"]

            if operation == "add":
                self.savings_data["current_amount"] += amount
                action = "存入"
                emoji = "➕"
            else:
                if amount > self.savings_data["current_amount"]:
                    messagebox.showwarning("操作错误", "减少金额不能超过当前已攒金额")
                    return
                self.savings_data["current_amount"] -= amount
                action = "取出"
                emoji = "➖"

            # 更新当前已攒输入框
            self.current_entry.delete(0, tk.END)
            self.current_entry.insert(0, f"{self.savings_data['current_amount']:.2f}")

            # 如果当前有选中目标，也更新目标数据
            if self.current_target_index >= 0:
                target = self.savings_data["targets"][self.current_target_index]
                target["current_amount"] = self.savings_data["current_amount"]
                self.refresh_targets_list()

            # 记录操作历史
            self.savings_data["savings_history"].append({
                "timestamp": datetime.now(),
                "amount": amount,
                "type": action,
                "balance_after": self.savings_data["current_amount"]
            })

            # 记录金额变化
            self.savings_data["amount_history"].append({
                "timestamp": datetime.now(),
                "amount": self.savings_data["current_amount"],
                "target_amount": self.savings_data["target_amount"]
            })

            self.add_message("系统",
                             f"{emoji} {action}金额：{'+' if operation == 'add' else '-'}¥{amount:,.2f}\n"
                             f"💰 余额更新：¥{old_amount:,.2f} → ¥{self.savings_data['current_amount']:,.2f}",
                             "system")
            self.amount_entry.delete(0, tk.END)
            self.update_progress_display()
            self.save_savings_data()

        except ValueError:
            messagebox.showerror("输入错误", "请输入有效的数字")

    def update_progress_display(self):
        """更新进度显示"""
        # 组件可能在切换/折叠时不存在，避免TclError
        if not (hasattr(self, "progress_label") and self.progress_label.winfo_exists() and
                hasattr(self, "time_label") and self.time_label.winfo_exists() and
                hasattr(self, "current_target_label") and self.current_target_label.winfo_exists()):
            return
        if self.savings_data["target_amount"] > 0:
            progress = min((self.savings_data["current_amount"] / self.savings_data["target_amount"]) * 100, 100)
            self.progress_var.set(progress)

            remaining = max(self.savings_data["target_amount"] - self.savings_data["current_amount"], 0)

            # 计算预计完成时间
            monthly_savings = self.savings_data["monthly_income"] - self.savings_data["monthly_expenses"]
            if monthly_savings > 0:
                months_needed = remaining / monthly_savings
                completion_date = datetime.now() + timedelta(days=months_needed * 30)
                time_text = f"⏰ 预计完成时间：{completion_date.strftime('%Y年%m月%d日')}（约{months_needed:.1f}个月）"
            else:
                time_text = "⚠️ 月净收入为负，无法计算完成时间"

            self.progress_label.config(
                text=f"📊 进度：{progress:.1f}% | 💰 已攒：¥{self.savings_data['current_amount']:,.2f} / ¥{self.savings_data['target_amount']:,.2f} | 🎯 还需：¥{remaining:,.2f}"
            )
            self.time_label.config(text=time_text)

            # 更新当前目标显示
            self.update_current_target_display()
        else:
            self.progress_var.set(0)
            self.progress_label.config(text="🎯 请先设置攒钱目标")
            self.time_label.config(text="")
            self.current_target_label.config(text="暂无选中目标", foreground='#F39C12')

    def cancel_requests(self):
        """取消所有活跃的请求"""
        if self.active_requests:
            self.status_var.set("🟡 正在取消请求...")
            self.active_requests.clear()
            self.advice_button.config(state=tk.NORMAL)
            self.cancel_button.config(state=tk.DISABLED)
            self.ai_progress_bar.stop()
            self.ai_progress_bar.pack_forget()
            self.add_message("系统", "❌ 请求已取消", "system")
            self.status_var.set("🟢 系统就绪")

    def clear_chat(self):
        """清空聊天记录"""
        if messagebox.askyesno("确认清空", "确定要清空所有聊天记录吗？"):
            self.chat_display.config(state=tk.NORMAL)
            self.chat_display.delete(1.0, tk.END)
            self.chat_display.config(state=tk.DISABLED)
            self.cancel_requests()
            welcome_msg = "🗑️ 聊天记录已清空！您可以继续咨询攒钱建议。"
            self.add_message("系统", welcome_msg, "system")

    def add_message(self, sender, message, msg_type="user"):
        """添加消息到聊天显示区域"""
        self.chat_display.config(state=tk.NORMAL)

        timestamp = time.strftime("%H:%M:%S")

        if msg_type == "user":
            self.chat_display.insert(tk.END, f"\n[{timestamp}] 👤 您: ", "user_tag")
            self.chat_display.insert(tk.END, f"{message}\n", "user_msg")
        elif msg_type == "assistant":
            self.chat_display.insert(tk.END, f"\n[{timestamp}] 🤖 AI助手: ", "assistant_tag")
            self.chat_display.insert(tk.END, f"{message}\n", "assistant_msg")
            self.last_ai_message = message
        else:
            self.chat_display.insert(tk.END, f"\n[{timestamp}] ⚙️ {sender}: ", "system_tag")
            self.chat_display.insert(tk.END, f"{message}\n", "system_msg")

        self.chat_display.see(tk.END)
        self.chat_display.config(state=tk.DISABLED)

    def update_advice_display(self, advice):
        """更新建议显示"""
        self.advice_text.config(state=tk.NORMAL)
        self.advice_text.delete(1.0, tk.END)
        self.advice_text.insert(1.0, advice)
        self.advice_text.config(state=tk.DISABLED)

    def get_savings_advice(self):
        """获取攒钱建议"""
        if self.savings_data["target_amount"] == 0:
            messagebox.showwarning("未设置目标", "请先设置攒钱目标")
            return

        self.advice_button.config(state=tk.DISABLED)
        self.cancel_button.config(state=tk.NORMAL)
        self.status_var.set("🟡 AI正在思考建议...")
        self.ai_progress_bar.pack(fill=tk.X, pady=5)
        self.ai_progress_bar.start()

        # 使用线程池提交任务
        future = self.thread_pool.submit(self.generate_savings_advice)
        request_id = id(future)
        self.active_requests.add(request_id)

        # 添加回调处理结果
        future.add_done_callback(lambda f: self.handle_advice_result(f, request_id))

    def handle_advice_result(self, future, request_id):
        """处理建议结果"""
        self.active_requests.discard(request_id)
        self.ai_progress_bar.stop()
        self.ai_progress_bar.pack_forget()

        try:
            result, error = future.result()

            if error:
                self.root.after(0, lambda: self.update_advice_display(f"❌ 抱歉，获取建议时发生了错误：{error}"))
            else:
                self.root.after(0, lambda: self.update_advice_display(result))
                self.root.after(0, lambda: self.add_message("AI助手", f"💡 {result}", "assistant"))

        except Exception as e:
            self.root.after(0, lambda: self.update_advice_display(f"❌ 处理结果时发生错误：{str(e)}"))

        finally:
            self.root.after(0, lambda: self.advice_button.config(state=tk.NORMAL))
            self.root.after(0, lambda: self.cancel_button.config(state=tk.DISABLED))
            self.root.after(0, lambda: self.status_var.set("🟢 系统就绪"))

    def generate_savings_advice(self):
        """生成攒钱建议 - 在单独的线程中运行"""
        try:
            # 模拟AI思考过程
            time.sleep(2)

            # 构建提示词
            prompt = f"""
            请根据以下攒钱情况提供专业建议：

            目标物品：{self.savings_data['target_item']}
            目标金额：¥{self.savings_data['target_amount']:,.2f}
            当前已攒：¥{self.savings_data['current_amount']:,.2f}
            月收入：¥{self.savings_data['monthly_income']:,.2f}
            月支出：¥{self.savings_data['monthly_expenses']:,.2f}
            月净收入：¥{self.savings_data['monthly_income'] - self.savings_data['monthly_expenses']:,.2f}
            当前进度：{(self.savings_data['current_amount'] / self.savings_data['target_amount'] * 100):.1f}%

            请提供：
            1. 进度分析和鼓励
            2. 具体的攒钱策略建议
            3. 支出优化建议
            4. 风险提示（如物品价格变化）
            5. 实用的省钱技巧

            请用中文回复，语气积极鼓励，建议具体可行。
            """

            response = self.client.chat.completions.create(
                model="deepseek-chat",
                messages=[
                    {"role": "system", "content": "你是一个专业的理财顾问，擅长提供攒钱和储蓄建议。"},
                    {"role": "user", "content": prompt},
                ],
                stream=False
            )

            return response.choices[0].message.content, None

        except Exception as e:
            return None, str(e)

    def run(self):
        """运行应用程序"""
        self.item_entry.focus_set()
        self.root.mainloop()

    def cleanup(self):
        """清理资源"""
        self.price_monitor_running = False
        self.amount_recording_running = False
        if hasattr(self, 'thread_pool'):
            self.thread_pool.shutdown(wait=False)


def main():
    """主函数"""
    try:
        root = tk.Tk()
        app = SavingsPlannerGUI(root)

        # 注册关闭事件
        root.protocol("WM_DELETE_WINDOW", lambda: on_closing(root, app))

        app.run()
    except Exception as e:
        messagebox.showerror("错误", f"程序启动失败：{str(e)}")


def on_closing(root, app):
    """处理窗口关闭事件"""
    app.cleanup()
    root.destroy()


if __name__ == "__main__":
    main()