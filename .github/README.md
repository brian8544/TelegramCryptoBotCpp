# TelegramCryptoBot

This project is a C# bot migrated to C++ as a hobby / self-exercise. It automates updates to your Telegram channel by retrieving real-time price data from CoinMarketCap, calculating price fluctuations, and delivering AI-driven market analysis.

## Features

- Cryptocurrency price tracking (BTC, XRP, ETH, SOL).
- Automatic price updates at configurable intervals.
- Price display in both EUR and USD.
- Price change percentage calculations.
- AI-powered market analysis using OpenRouter API.

## Prerequisites

- C++17 compiler (Confirmed working on Windows)
- libcurl
- nlohmann/json
- TgBot library
- CMake

## API Keys Required

1. Telegram Bot Token - Get it from [BotFather](https://core.telegram.org/bots/tutorial)
2. CoinMarketCap API Key - Get it from [CoinMarketCap Pro](https://pro.coinmarketcap.com/account)
3. OpenRouter API Key (Optional, for AI analysis) - Get it from [OpenRouter](https://openrouter.ai/settings/keys)

## Running the Bot

1. Build the project
2. Configure: `Config.conf`
3. Run the executable: `TelegramCryptoBot.exe`

## Bot Output Example

```
🔄 Crypto Update • 15:30 13-01-2025
⏳ Next update at: 16:30

₿ BTC: €40,123.45 | $43,567.89 📈 +2.5%
⟠ ETH: €2,234.56 | $2,456.78 📈 +1.8%
◎ SOL: €89.12 | $98.76 📉 -0.5%
✖ XRP: €0.56 | $0.62 📈 +3.2%

📊 Quick Analysis:
[AI-generated market analysis appears here]
```

## Disclaimer

This bot is provided as-is for educational and hobby purposes only. I am NOT responsible for any financial losses or damages caused by the use of this bot. Use at your own risk. Cryptocurrency trading is volatile, and market conditions can change rapidly.

## Contributing

This is a hobby project, and I welcome contributions of all kinds, feel free to:
- Open issues for bugs or feature requests
- Submit pull requests with improvements
- Share ideas for new features
- Suggest code improvements or optimizations

## License

License: GPL 2.0