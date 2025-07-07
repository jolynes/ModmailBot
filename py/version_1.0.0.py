import discord
from discord.ext import commands
from discord.ui import View, Button, Modal, TextInput
import datetime

GUILD_ID = ts server_id  # Replace with your server ID
MODMAIL_CATEGORY_NAME = "ModMail" # create category named ModMail or change it urself
bot_token = "paste token" # Replace with your token

intents = discord.Intents.default()
intents.messages = True
intents.guilds = True
intents.dm_messages = True
intents.message_content = True
intents.members = True

bot = commands.Bot(command_prefix="!", intents=intents)
ticket_start_times = {}  # Store when a modmail channel is created

class CloseReasonModal(Modal, title="Close ModMail Ticket"):
    reason = TextInput(label="Reason for closing", style=discord.TextStyle.paragraph)

    def __init__(self, user, staff, channel):
        super().__init__()
        self.user = user
        self.staff = staff
        self.channel = channel

    async def on_submit(self, interaction: discord.Interaction):
        now = datetime.datetime.utcnow()
        open_time = ticket_start_times.get(self.channel.id)
        duration = now - open_time if open_time else None
        formatted_duration = str(duration).split('.')[0] if duration else "Unknown"

        embed = discord.Embed(title="\ud83d\udceb ModMail Closed", color=discord.Color.red())
        embed.add_field(name="Closed By", value=f"{self.staff} (`{self.staff.id}`)", inline=False)
        embed.add_field(name="Time Open", value=formatted_duration, inline=False)
        embed.add_field(name="Reason", value=self.reason.value, inline=False)
        embed.timestamp = now

        try:
            await self.user.send(embed=embed)
        except:
            await interaction.channel.send("\u26a0\ufe0f Could not notify the user.")

        log_channel = discord.utils.get(interaction.guild.text_channels, name="modmail-log")
        if log_channel:
            log_embed = embed.copy()
            log_embed.title = "\ud83d\uddd1\ufe0f Ticket Closed"
            log_embed.description = f"Thread: {self.channel.mention} was closed."
            await log_channel.send(embed=log_embed)

        await interaction.response.send_message("\u2705 Ticket closed.", ephemeral=True)
        await self.channel.delete()

class CloseTicketView(View):
    def __init__(self, user, staff, channel):
        super().__init__(timeout=None)
        self.user = user
        self.staff = staff
        self.channel = channel

    @discord.ui.button(label="Close", style=discord.ButtonStyle.danger, emoji="\ud83d\uddd1\ufe0f", custom_id="close_button")
    async def close(self, interaction: discord.Interaction, button: discord.ui.Button):
        if not interaction.user.guild_permissions.manage_channels:
            await interaction.response.send_message("You do not have permission to close this.", ephemeral=True)
            return
        modal = CloseReasonModal(user=self.user, staff=interaction.user, channel=self.channel)
        await interaction.response.send_modal(modal)

@bot.event
async def on_ready():
    print(f"\u2705 Bot online as {bot.user}")

@bot.event
async def on_message(message):
    if message.author.bot:
        return

    if isinstance(message.channel, discord.DMChannel):
        guild = bot.get_guild(GUILD_ID)
        if guild is None:
            return

        category = discord.utils.get(guild.categories, name=MODMAIL_CATEGORY_NAME)
        if category is None:
            category = await guild.create_category(MODMAIL_CATEGORY_NAME)

        channel_name = f"modmail-{message.author.id}"
        existing_channel = discord.utils.get(guild.text_channels, name=channel_name)

        if not existing_channel:
            new_channel = await guild.create_text_channel(channel_name, category=category)
            await new_channel.send(
                f"\ud83d\udcec New ModMail started by {message.author.mention}",
                view=CloseTicketView(user=message.author, staff=bot.user, channel=new_channel)
            )
            await new_channel.send(f"**{message.author}:** {message.content}")
            for a in message.attachments:
                await new_channel.send(file=await a.to_file())
            await message.author.send("\u2705 Your message has been sent to the support team!")
            ticket_start_times[new_channel.id] = datetime.datetime.utcnow()

            log_channel = discord.utils.get(guild.text_channels, name="modmail-log")
            if log_channel:
                embed = discord.Embed(title="\ud83d\udcc9 New Ticket", color=discord.Color.blurple())
                timestamp = discord.utils.format_dt(datetime.datetime.utcnow(), style='f')
                embed.description = f"_{message.author} | `{message.author.id}` \u2022 {timestamp}"
                await log_channel.send(embed=embed)

        else:
            await existing_channel.send(f"**{message.author}:** {message.content}")
            for a in message.attachments:
                await existing_channel.send(file=await a.to_file())

    elif message.guild and message.channel.category and message.channel.category.name == MODMAIL_CATEGORY_NAME:
        try:
            user_id = int(message.channel.name.replace("modmail-", ""))
            user = await bot.fetch_user(user_id)
            if message.content:
                await user.send(message.content)
            for a in message.attachments:
                await user.send(file=await a.to_file())
        except:
            await message.channel.send("\u274c Could not send message to user.")

    await bot.process_commands(message)

@bot.command()
@commands.has_permissions(manage_channels=True)
async def close(ctx, *, reason: str = None):
    if not ctx.channel.category or ctx.channel.category.name != MODMAIL_CATEGORY_NAME:
        await ctx.send("\u274c This command can only be used in a modmail thread.")
        return

    if not reason:
        await ctx.send("\u274c You must provide a reason. Usage: `!close <reason>`")
        return

    try:
        await ctx.message.delete()
    except:
        pass

    try:
        user_id = int(ctx.channel.name.replace("modmail-", ""))
        user = await bot.fetch_user(user_id)

        start_time = ticket_start_times.get(ctx.channel.id)
        now = datetime.datetime.utcnow()
        duration = now - start_time if start_time else None
        formatted_duration = str(duration).split('.')[0] if duration else "Unknown"

        embed = discord.Embed(title="\ud83d\udceb ModMail Closed", color=discord.Color.red())
        embed.add_field(name="Closed By", value=f"{ctx.author} (`{ctx.author.id}`)", inline=False)
        embed.add_field(name="Time Open", value=formatted_duration, inline=False)
        embed.add_field(name="Reason", value=reason, inline=False)
        embed.timestamp = now

        await user.send(embed=embed)

        log_channel = discord.utils.get(ctx.guild.text_channels, name="modmail-log")
        if log_channel:
            log_embed = embed.copy()
            log_embed.title = "\ud83d\uddd1\ufe0f Ticket Closed"
            log_embed.description = f"Thread: {ctx.channel.mention} was closed."
            await log_channel.send(embed=log_embed)

    except:
        await ctx.send("\u26a0\ufe0f Could not DM the user.")

    await ctx.send("\u2705 Ticket closed.")
    await ctx.channel.delete()

bot.run(bot_token)
